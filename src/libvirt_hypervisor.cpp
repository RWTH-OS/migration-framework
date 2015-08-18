/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "libvirt_hypervisor.hpp"

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include <stdexcept>
#include <thread>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>

struct Deleter_virConnect
{
	void operator()(virConnectPtr ptr) const
	{
		virConnectClose(ptr);
	}
};

struct Deleter_virDomain
{
	void operator()(virDomainPtr ptr) const
	{
		virDomainFree(ptr);
	}
};

std::string convert_and_free_cstr(char *cstr)
{
	std::string str;
	if (cstr) {
		str.assign(cstr);
		free(cstr);
	}
	return str;

}

template<typename T, template<typename> class P = std::unique_ptr> std::vector<P<T>> 
convert_and_free_ptr_array(T **carray, size_t length)
{
	std::vector<P<T>> vec;
	if (length != 0)
		vec.assign(carray, carray + length);
	if (carray)
		free(carray);
	return vec;
}

class Migrate_devices_guard
{
};

struct Device
{
	Device(const std::string &xml_desc);
	Device(std::string &&xml_desc);
	const std::string xml_desc;
	std::atomic<bool> attached_hint;
};

Device::Device(const std::string &xml_desc) :
	xml_desc(xml_desc), attached_hint(false)
{
}

Device::Device(std::string &&xml_desc) :
	xml_desc(std::move(xml_desc)), attached_hint(false)
{
}

class Device_cache
{
public:
	std::vector<std::shared_ptr<Device>> get_devices(virConnectPtr host_connection, short type_id) const;
private:
	// (hosturi : (type_id : xml_desc))
	mutable std::unordered_map<std::string, std::unordered_map<short, std::vector<std::shared_ptr<Device>>>> devices;
	mutable std::mutex devices_mutex;
};

std::vector<std::shared_ptr<Device>> Device_cache::get_devices(virConnectPtr host_connection, short type_id) const
{
	// Get host URI and convert to std::string.
	auto host_uri = convert_and_free_cstr(virConnectGetURI(host_connection));
	// Lock while accessing devices cache.
	std::unique_lock<std::mutex> lock(devices_mutex);
	// If no entry found try to find and cache devices.
	if (devices[host_uri][type_id].empty()) {
		virNodeDevicePtr *devices_carray = nullptr;
		auto ret = virConnectListAllNodeDevices(host_connection, 
							&devices_carray, 
							VIR_CONNECT_LIST_NODE_DEVICES_CAP_PCI_DEV);
		if (ret == -1) { // Libvirt error
			throw std::runtime_error("Error collecting list of node devices.");
		}
		// Convert c-array to std::vectory<> and pointer to unique_ptr
		std::vector<std::unique_ptr<virNodeDevice>> found_devices = convert_and_free_ptr_array(devices_carray, ret);
		// Fill cache with found devices
		for (auto &device : found_devices) {
			auto xml_desc = convert_and_free_cstr(virNodeDeviceGetXMLDesc(device.get(), 0));
			devices[host_uri][type_id].push_back(std::make_shared<Device>(std::move(xml_desc)));
		}
	}
	// Copy devices from cache.
	auto vec =  devices[host_uri][type_id];
	// Unlock since no access to devices cache is needed anymore.
	lock.unlock();
	// Sort potentially attached devices to end of vector.
	std::sort(vec.begin(), vec.end(), 
			[](const std::shared_ptr<Device> &lhs, const std::shared_ptr<Device> &rhs)
			{
				return lhs->attached_hint < rhs->attached_hint;
			});
	return vec;
}

// TODO: Caching class to query pci xml descs
class PCI_device_handler
{
public:
	/**
	 * \brief Attach device of certain type to domain.
	 */
	void attach(virDomainPtr domain, short device_type);
	/**
	 * \brief Detach device of certain type to domain.
	 */
	void Detach(virDomainPtr domain, short device_type);
	/**
	 * \brief Returns a RAII-guard for detaching devices before and attaching after migration.
	 */
	Migrate_devices_guard migrate_device_wrapper();
private:
	std::unique_ptr<const Device_cache> device_cache;	
};

template<typename T, typename std::enable_if<std::is_integral<T>{}>::type* = nullptr> 
std::string to_hex_string(const T &integer)
{
	std::stringstream ss;
	ss << std::hex << integer;
	return ss.str();
}

void PCI_device_handler::attach(virDomainPtr domain, short device_type)
{
	// Get connection the domain belongs to.
	auto connection = virDomainGetConnect(domain);
	// Get vector of devices.
	auto devices = device_cache->get_devices(connection, device_type);
	if (devices.empty()) {
		throw std::runtime_error("No devices of type \"" + to_hex_string(device_type) 
			+ "\" found on \"" + convert_and_free_cstr(virConnectGetURI(connection)) + "\".");
	}
	// Try to attach a device until success or none is left.
	int ret = -1;
	for (auto &device : devices) {
		ret = virDomainAttachDevice(domain, device->xml_desc.c_str());
		device->attached_hint = true;
		if (ret == 0)
			break;
	}
	if (ret != 0)
		throw std::runtime_error("No pci device could be attached");
}

const std::string file_name = "devices/ib_pci_82_00_1.xml";

void attach_device(virDomainPtr domain)
{
	// Convert config file to string
	std::ifstream file_stream(file_name);
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf(); // Filestream to stingstream conversion
	auto pci_device_xml = string_stream.str();

	// attach device
	auto ret = virDomainAttachDevice(domain, pci_device_xml.c_str());
	if (ret != 0)
		throw std::runtime_error("Failed attaching device with following xml:\n" + pci_device_xml);
}

void detach_device(virDomainPtr domain)
{
	// Convert config file to string
	std::ifstream file_stream(file_name);
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf(); // Filestream to stingstream conversion
	auto pci_device_xml = string_stream.str();

	// attach device
	auto ret = virDomainDetachDevice(domain, pci_device_xml.c_str());
	if (ret != 0)
		throw std::runtime_error("Failed detaching device with following xml:\n" + pci_device_xml);
}



/// TODO: Get hostname dynamically.
Libvirt_hypervisor::Libvirt_hypervisor() :
	local_host_conn(virConnectOpen("qemu:///system"))
{
	if (!local_host_conn)
		throw std::runtime_error("Failed to connect to qemu on local host.");
}

Libvirt_hypervisor::~Libvirt_hypervisor()
{
	if (virConnectClose(local_host_conn)) {
		std::cout << "Warning: Some qemu connections have not been closed after destruction of hypervisor wrapper!" << std::endl;
	}
}

void Libvirt_hypervisor::start(const std::string &vm_name, unsigned int vcpus, unsigned long memory)
{
	// Get domain by name
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in shutdown state
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_SHUTOFF)
		throw std::runtime_error("Wrong domain state: " + std::to_string(domain_info.state));
	// Set memory
	if (virDomainSetMemoryFlags(domain.get(), memory, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_MEM_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum amount of memory to " + std::to_string(memory) + " KiB for domain " + vm_name);
	if (virDomainSetMemoryFlags(domain.get(), memory, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting amount of memory to " + std::to_string(memory) + " KiB for domain " + vm_name);
	// Set VCPUs
	if (virDomainSetVcpusFlags(domain.get(), vcpus, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_VCPU_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum number of vcpus to " + std::to_string(vcpus) + " for domain " + vm_name);
	if (virDomainSetVcpusFlags(domain.get(), vcpus, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting number of vcpus to " + std::to_string(vcpus) + " for domain " + vm_name);
	// Create domain
	if (virDomainCreate(domain.get()) == -1)
		throw std::runtime_error(std::string("Error creating domain: ") + virGetLastErrorMessage());
	// Attach device
	attach_device(domain.get());
}

void Libvirt_hypervisor::stop(const std::string &vm_name)
{
	// Get domain by name
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in running state
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_RUNNING)
		throw std::runtime_error("Domain not running.");
	// Destroy domain
	if (virDomainDestroy(domain.get()) == -1)
		throw std::runtime_error("Error destroying domain.");
}

void Libvirt_hypervisor::migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool rdma_migration)
{
	// Get domain by name
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in running state
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_RUNNING)
		throw std::runtime_error("Domain not running.");
	// Detach devices TODO: RAII handler and dynamic device recognition.
	detach_device(domain.get());
	// Connect to destination
	std::unique_ptr<virConnect, Deleter_virConnect> dest_connection(
		virConnectOpen(("qemu+ssh://" + dest_hostname + "/system").c_str())
	);
	if (!dest_connection)
		throw std::runtime_error("Cannot establish connection to " + dest_hostname);
	// Set migration flags
	unsigned long flags = 0;
	flags |= live_migration ? VIR_MIGRATE_LIVE : 0;
	// create migrateuri
	std::string migrate_uri = rdma_migration? "rdma://" + dest_hostname + "-ib" : NULL;
	// Migrate domain
	std::unique_ptr<virDomain, Deleter_virDomain> dest_domain(
		virDomainMigrate(domain.get(), dest_connection.get(), flags, 0, migrate_uri.c_str(), 0)
	);
	if (!dest_domain)
		throw std::runtime_error(std::string("Migration failed: ") + virGetLastErrorMessage());
	// Attach device
	attach_device(dest_domain.get());
}
