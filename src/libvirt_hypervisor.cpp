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
#include <boost/log/trivial.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/regex.hpp>

#include <stdexcept>
#include <thread>
#include <memory>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>

// Some deleter to be used with smart pointers.

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

struct Deleter_virNodeDevice
{
	void operator()(virNodeDevicePtr ptr) const
	{
		virNodeDeviceFree(ptr);
	}
};

// Libvirt sometimes returns a dynamically allocated cstring.
// As we prefer std::string this function converts and frees.
std::string convert_and_free_cstr(char *cstr)
{
	std::string str;
	if (cstr) {
		str.assign(cstr);
		free(cstr);
	}
	return str;

}

// Wraps ugly passing of C style array of raw pointers to returning vector of smart pointers.
std::vector<std::unique_ptr<virNodeDevice, Deleter_virNodeDevice>> list_all_node_devices_wrapper(virConnectPtr conn, unsigned int flags)
{
	virNodeDevicePtr *devices_carray = nullptr;
	auto ret = virConnectListAllNodeDevices(conn, &devices_carray, flags);
	if (ret < 0) // Libvirt error
		throw std::runtime_error("Error collecting list of node devices.");
	std::vector<std::unique_ptr<virNodeDevice, Deleter_virNodeDevice>> devices_vec(devices_carray, devices_carray + ret);
	if (devices_carray)
		free(devices_carray);
	return devices_vec;
}

// Converts integer type numbers to string in hex format.
template<typename T, typename std::enable_if<std::is_integral<T>{}>::type* = nullptr> 
std::string to_hex_string(const T &integer, int digits)
{
	std::stringstream ss;
	ss << "0x" << std::hex << std::setfill('0') << std::setw(digits) << +integer;
	return ss.str();
}

// Convert xml string to ptree
boost::property_tree::ptree read_xml_from_string(const std::string &str)
{
	boost::property_tree::ptree pt;
	std::stringstream ss(str);
	read_xml(ss, pt);
	return pt;
}

// Contains pci address and methods to convert from and to ptree.
struct PCI_address
{
	PCI_address(unsigned short domain, unsigned char bus, unsigned char slot, unsigned char function);
	PCI_address(const boost::property_tree::ptree &device_ptree);

	boost::property_tree::ptree to_ptree() const;
	std::string str() const;

	const unsigned short domain;
	const unsigned char bus;
	const unsigned char slot;
	const unsigned char function;
};

// Contains xml description of device which can be used to attach/detach.
// Also contains a hint to mark the device as already in use.
// Nevertheless the device might still be tried to attach but has lower priority.
struct Device
{
	Device(const std::string &xml_desc);
	Device(std::string &&xml_desc);

	std::string to_hostdev_xml() const;

	const std::string xml_desc;
	const PCI_address address;
	std::atomic<bool> attached_hint;
};

// A lazy initialized cache to store devices in.
class Device_cache
{
public:
	std::vector<std::shared_ptr<Device>> get_devices(virConnectPtr host_connection, unsigned short type_id) const;
private:
	// (hosturi : (type_id : xml_desc))
	mutable std::unordered_map<std::string, std::unordered_map<short, std::vector<std::shared_ptr<Device>>>> devices;
	mutable std::mutex devices_mutex;
};

// Provides methods to attach, detach and handle those during migration.
// TODO: Improve use of PCI device vendor and type id 
class PCI_device_handler
{
public:
	PCI_device_handler();
	/**
	 * \brief Attach device of certain type to domain.
	 */
	void attach(virDomainPtr domain, short device_type);
	/**
	 * \brief Detach device of certain type to domain.
	 */
	void detach(virDomainPtr domain, short device_type);
private:
	std::unique_ptr<const Device_cache> device_cache;	
};

// RAII-guard to detach devices in constructor and reattach in destructor.
// If no error occures during migration the domain on destination should be set.
class Migrate_devices_guard
{
public:
	Migrate_devices_guard(std::shared_ptr<PCI_device_handler> pci_device_handler);
	~Migrate_devices_guard();
	void set_dest_domain(virDomainPtr);
private:
	virDomainPtr domain;
	std::shared_ptr<PCI_device_handler> pci_device_handler;
};

//
// PCI_address implementation
//

PCI_address::PCI_address(unsigned short domain, unsigned char bus, unsigned char slot, unsigned char function) :
	domain(domain), bus(bus), slot(slot), function(function)
{
}

PCI_address::PCI_address(const boost::property_tree::ptree &device_ptree) :
	domain(device_ptree.get<unsigned short>("device.capability.domain")),
	bus(device_ptree.get<unsigned char>("device.capability.bus")),
	slot(device_ptree.get<unsigned char>("device.capability.slot")),
	function(device_ptree.get<unsigned char>("device.capability.function"))
{
}

boost::property_tree::ptree PCI_address::to_ptree() const
{
	boost::property_tree::ptree pt;
	pt.put("address.<xmlattr>.domain", to_hex_string(domain, 4));
	pt.put("address.<xmlattr>.bus", to_hex_string(bus, 2));
	pt.put("address.<xmlattr>.slot", to_hex_string(slot, 2));
	pt.put("address.<xmlattr>.function", to_hex_string(function, 1));
	return pt;
}

std::string PCI_address::str() const
{
	return to_hex_string(domain, 4) + ":" + to_hex_string(bus, 2) + ":" + to_hex_string(slot, 2) + "." + to_hex_string(function, 1);
}

//
// Device implementation
//

Device::Device(const std::string &xml_desc) :
	xml_desc(xml_desc),
	address(read_xml_from_string(xml_desc)),
	attached_hint(false)
{
}

Device::Device(std::string &&xml_desc) :
	xml_desc(std::move(xml_desc)),
	address(read_xml_from_string(this->xml_desc)),
	attached_hint(false)
{
}

std::string Device::to_hostdev_xml() const
{
	using namespace boost::property_tree;
	// Write hostdev xml
	ptree hostdev_ptree;
	hostdev_ptree.put("hostdev.<xmlattr>.mode", "subsystem");
	hostdev_ptree.put("hostdev.<xmlattr>.type", "pci");
	hostdev_ptree.put("hostdev.<xmlattr>.managed", "yes");
	hostdev_ptree.put_child("hostdev.source", address.to_ptree());

	std::stringstream hostdev_xml_sstream;
	xml_parser::xml_writer_settings<std::string> settings('\t', 1); // Make output pretty (indention)
	write_xml(hostdev_xml_sstream, hostdev_ptree, settings);
	// Remove xml tag
	boost::regex xml_tag_regex(R"((^<\?xml.*version.*encoding.*\?>\n?))");
	return boost::regex_replace(hostdev_xml_sstream.str(), xml_tag_regex, "");
}

//
// Migrate_devices_guard implementation
//

Migrate_devices_guard::Migrate_devices_guard(std::shared_ptr<PCI_device_handler> pci_device_handler) :
	pci_device_handler(pci_device_handler)
{
	// detach all devs and store list of detached dev types
}

Migrate_devices_guard::~Migrate_devices_guard()
{
	// reattach stored dev types on destination
}

void Migrate_devices_guard::set_dest_domain(virDomainPtr dest_domain)
{
	(void) dest_domain; 
	// override domain to reattach devices on
}

//
// Device_cache implementation
//

std::vector<std::shared_ptr<Device>> Device_cache::get_devices(virConnectPtr host_connection, unsigned short type_id) const
{
	auto host_uri = convert_and_free_cstr(virConnectGetURI(host_connection));
	auto type_id_hex = to_hex_string(type_id, 4);
	BOOST_LOG_TRIVIAL(trace) << "Get devices on host=" << host_uri << " with type_id=" << type_id_hex;
	BOOST_LOG_TRIVIAL(trace) << "Lock while accessing device cache.";
	std::unique_lock<std::mutex> lock(devices_mutex);
	// If no entry found try to find and cache devices.
	if (devices[host_uri][type_id].empty()) {
		BOOST_LOG_TRIVIAL(trace) << "No entry found.";
		// Find devices
		BOOST_LOG_TRIVIAL(trace) << "Search for devices.";
		auto found_devices = list_all_node_devices_wrapper(host_connection, VIR_CONNECT_LIST_NODE_DEVICES_CAP_PCI_DEV);
		// Fill cache with found devices
		BOOST_LOG_TRIVIAL(trace) << "Filtering " << found_devices.size() << " found PCI devices.";
		for (auto &device : found_devices) {
			auto xml_desc = convert_and_free_cstr(virNodeDeviceGetXMLDesc(device.get(), 0));
			if (xml_desc.find("<product id='" + type_id_hex + "'>") != std::string::npos) {
				auto dev = std::make_shared<Device>(std::move(xml_desc));
				BOOST_LOG_TRIVIAL(trace) << "Adding device: " << dev->address.str();
				devices[host_uri][type_id].push_back(dev);
			}
		}
	}
	// Copy devices from cache.
	auto vec =  devices[host_uri][type_id];
	BOOST_LOG_TRIVIAL(trace) << "Found " << vec.size() << " devices on cache.";
	// Unlock since no access to devices cache is needed anymore.
	BOOST_LOG_TRIVIAL(trace) << "Unlock since no access to device cache is needed anymore.";
	lock.unlock();
	// Sort potentially attached devices to end of vector.
	BOOST_LOG_TRIVIAL(trace) << "Sort potentially attached devices to end of vector.";
	std::sort(vec.begin(), vec.end(), 
			[](const std::shared_ptr<Device> &lhs, const std::shared_ptr<Device> &rhs)
			{
				return lhs->attached_hint < rhs->attached_hint;
			});
	// TODO: Shuffle unattached.
	return vec;
}

//
// PCI_device_handler implementation
//

PCI_device_handler::PCI_device_handler() :
	device_cache(new Device_cache)
{
}

void PCI_device_handler::attach(virDomainPtr domain, short device_type)
{
	// Get connection the domain belongs to.
	BOOST_LOG_TRIVIAL(trace) << "Get connection the domain belongs to.";
	auto connection = virDomainGetConnect(domain);
	// Get vector of devices.
	BOOST_LOG_TRIVIAL(trace) << "Get vector of devices.";
	auto devices = device_cache->get_devices(connection, device_type);
	if (devices.empty()) {
		throw std::runtime_error("No devices of type \"" + to_hex_string(device_type, 4) 
			+ "\" found on \"" + convert_and_free_cstr(virConnectGetURI(connection)) + "\".");
	}
	// Try to attach a device until success or none is left.
	BOOST_LOG_TRIVIAL(trace) << "Try to attach a device until success or none is left.";
	int ret = -1;
	for (auto &device : devices) {
		BOOST_LOG_TRIVIAL(trace) << "Trying to attach device " << device->address.str();
		auto hostdev_xml = device->to_hostdev_xml();
		BOOST_LOG_TRIVIAL(trace) << "Hostdev xml:";
		BOOST_LOG_TRIVIAL(trace) << hostdev_xml;
		ret = virDomainAttachDevice(domain, hostdev_xml.c_str());
		device->attached_hint = true;
		if (ret == 0) {
			BOOST_LOG_TRIVIAL(trace) << "Success attaching device.";
			break;
		}
		BOOST_LOG_TRIVIAL(trace) << "No success attaching device.";
	}
	if (ret != 0)
		throw std::runtime_error("No pci device could be attached");
}

void PCI_device_handler::detach(virDomainPtr domain, short device_type)
{
	(void) domain; (void) device_type;
	// parse domain xml getting all hostdevs
	// find those in cache, detach and set hint
	// return detached devices (may help reattaching on dest host)
}

//
// Libvirt_hypervisor implementation
//

/// TODO: Get hostname dynamically.
Libvirt_hypervisor::Libvirt_hypervisor() :
	local_host_conn(virConnectOpen("qemu:///system")),
	pci_device_handler(std::make_shared<PCI_device_handler>())
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
	BOOST_LOG_TRIVIAL(trace) << "Get domain by name.";
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in shutdown state
	BOOST_LOG_TRIVIAL(trace) << "Get domain info + check if in shutdown state.";
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_SHUTOFF)
		throw std::runtime_error("Wrong domain state: " + std::to_string(domain_info.state));
	// Set memory
	BOOST_LOG_TRIVIAL(trace) << "Set memory.";
	if (virDomainSetMemoryFlags(domain.get(), memory, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_MEM_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum amount of memory to " + std::to_string(memory) + " KiB for domain " + vm_name);
	if (virDomainSetMemoryFlags(domain.get(), memory, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting amount of memory to " + std::to_string(memory) + " KiB for domain " + vm_name);
	// Set VCPUs
	BOOST_LOG_TRIVIAL(trace) << "Set VCPUs.";
	if (virDomainSetVcpusFlags(domain.get(), vcpus, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_VCPU_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum number of vcpus to " + std::to_string(vcpus) + " for domain " + vm_name);
	if (virDomainSetVcpusFlags(domain.get(), vcpus, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting number of vcpus to " + std::to_string(vcpus) + " for domain " + vm_name);
	// Create domain
	BOOST_LOG_TRIVIAL(trace) << "Create domain.";
	if (virDomainCreate(domain.get()) == -1)
		throw std::runtime_error(std::string("Error creating domain: ") + virGetLastErrorMessage());
	// Attach device
	BOOST_LOG_TRIVIAL(trace) << "Attach device.";
	pci_device_handler->attach(domain.get(), 0x1004);
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
	// Guard migration of PCI devices.
	Migrate_devices_guard dev_guard(pci_device_handler);
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
	// Register dest_domain to Migrate_devices_guard to ensure device is reattached on destination.
	dev_guard.set_dest_domain(dest_domain.get());
}
