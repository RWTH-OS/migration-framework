/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "pci_device_handler.hpp"

#include <boost/log/trivial.hpp>
#include <boost/property_tree/xml_parser.hpp>
//#include <boost/regex.hpp>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>

// Some deleter to be used with smart pointers.

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
std::string to_hex_string(const T &integer, int digits, bool show_base = true)
{
	std::stringstream ss;
	ss << (show_base ? "0x" : "") << std::hex << std::setfill('0') << std::setw(digits) << +integer;
	return ss.str();
}

// Convert xml string to ptree.
boost::property_tree::ptree read_xml_from_string(const std::string &str)
{
	boost::property_tree::ptree pt;
	std::stringstream ss(str);
	read_xml(ss, pt, boost::property_tree::xml_parser::trim_whitespace);
	return pt;
}

// Convert ptree to xml string.
std::string write_xml_to_string(const boost::property_tree::ptree &ptree, bool pretty = true)
{
	std::stringstream ss;
	if (pretty) {
		boost::property_tree::xml_parser::xml_writer_settings<std::string> settings('\t', 1);
		write_xml(ss, ptree, settings);
	} else {
		write_xml(ss, ptree);
	}
	return ss.str();
}

//
// PCI_id implementation
//

PCI_id make_pci_id_from_nodedev_xml(const std::string &nodedev_xml)
{
	auto nodedev_ptree = read_xml_from_string(nodedev_xml);
	auto vendor = std::stoul(nodedev_ptree.get<std::string>("device.capability.vendor.<xmlattr>.id"), nullptr, 0);
	auto device = std::stoul(nodedev_ptree.get<std::string>("device.capability.product.<xmlattr>.id"), nullptr, 0);
	return PCI_id(vendor, device);
}

PCI_id::PCI_id(vendor_t vendor, device_t device) :
	vendor(vendor), device(device)
{
}

bool PCI_id::operator==(const PCI_id &rhs) const
{
	return vendor == rhs.vendor && device == rhs.device;
}

std::string PCI_id::vendor_hex() const
{
	return to_hex_string(vendor, 4);
}

std::string PCI_id::device_hex() const
{
	return to_hex_string(device, 4);
}

std::string PCI_id::str() const
{
	return to_hex_string(vendor, 4, false) + ":" + to_hex_string(device, 4, false);
}

YAML::Node PCI_id::emit() const
{
	YAML::Node node;
	node["vendor"] = vendor_hex();
	node["device"] = device_hex();
	return node;
}

void PCI_id::load(const YAML::Node &node)
{
	vendor = std::stoul(node["vendor"].as<std::string>(), nullptr, 0);
	device = std::stoul(node["device"].as<std::string>(), nullptr, 0);
}

//
// PCI_address implementation
//

PCI_address make_pci_address_from_device_ptree(const boost::property_tree::ptree &device_ptree)
{
	auto domain = device_ptree.get<PCI_address::domain_t>("device.capability.domain");
	auto bus = device_ptree.get<PCI_address::bus_t>("device.capability.bus");
	auto slot = device_ptree.get<PCI_address::slot_t>("device.capability.slot");
	auto function = device_ptree.get<PCI_address::function_t>("device.capability.function");
	return PCI_address(domain, bus, slot, function);
}

PCI_address make_pci_address_from_address_ptree(const boost::property_tree::ptree &address_ptree)
{
	auto xmlattr = address_ptree.get_child("address.<xmlattr>"); 
	// std::stoul has to be used since boost.property_tree cannot convert hex strings to integer types.
	auto domain = std::stoul(xmlattr.get<std::string>("domain"), nullptr, 0);
	auto bus = std::stoul(xmlattr.get<std::string>("bus"), nullptr, 0);
	auto slot = std::stoul(xmlattr.get<std::string>("slot"), nullptr, 0);
	auto function = std::stoul(xmlattr.get<std::string>("function"), nullptr, 0);
	return PCI_address(domain, bus, slot, function);
}

PCI_address::PCI_address(domain_t domain, bus_t bus, slot_t slot, function_t function) :
	domain(domain), bus(bus), slot(slot), function(function)
{
}

boost::property_tree::ptree PCI_address::to_address_ptree() const
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
	return to_hex_string(domain, 4, false) + ":"
		+ to_hex_string(bus, 2, false) + ":"
		+ to_hex_string(slot, 2, false) + "."
		+ to_hex_string(function, 1, false);
}

std::string PCI_address::to_name_fmt() const
{
	return "pci_" + to_hex_string(domain, 4, false) + "_"
		+ to_hex_string(bus, 2, false) + "_"
		+ to_hex_string(slot, 2, false) + "_"
		+ to_hex_string(function, 1, false);
}

bool PCI_address::operator==(const PCI_address &rhs) const
{
	return domain == rhs.domain && bus == rhs.bus && slot == rhs.slot && function == rhs.function;
}

//
// Device implementation
//

Device::Device(const std::string &xml_desc) :
	xml_desc(xml_desc),
	address(make_pci_address_from_device_ptree(read_xml_from_string(xml_desc))),
	attached_hint(false)
{
}

Device::Device(std::string &&xml_desc) :
	xml_desc(std::move(xml_desc)),
	address(make_pci_address_from_device_ptree(read_xml_from_string(this->xml_desc))),
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
	hostdev_ptree.put_child("hostdev.source", address.to_address_ptree());

	// Remove xml tag
	//boost::regex xml_tag_regex(R"((^<\?xml.*version.*encoding.*\?>\n?))");
	//return boost::regex_replace(hostdev_xml_sstream.str(), xml_tag_regex, "");
	return write_xml_to_string(hostdev_ptree);
}

//
// Device_cache implementation
//

std::vector<std::shared_ptr<Device>> Device_cache::get_devices(virConnectPtr host_connection, PCI_id pci_id, bool sort_and_shuffle) const
{
	auto host_uri = convert_and_free_cstr(virConnectGetURI(host_connection));
	BOOST_LOG_TRIVIAL(trace) << "Get devices on host " << host_uri << " with pci_id " << pci_id.str();
	BOOST_LOG_TRIVIAL(trace) << "Lock while accessing device cache.";
	std::unique_lock<std::mutex> lock(devices_mutex);
	// If no entry found try to find and cache devices.
	if (devices[host_uri][pci_id].empty()) {
		BOOST_LOG_TRIVIAL(trace) << "No entry found.";
		// Find devices
		BOOST_LOG_TRIVIAL(trace) << "Search for devices.";
		auto found_devices = list_all_node_devices_wrapper(host_connection, VIR_CONNECT_LIST_NODE_DEVICES_CAP_PCI_DEV);
		// Fill cache with found devices
		BOOST_LOG_TRIVIAL(trace) << "Filtering " << found_devices.size() << " found PCI devices.";
		for (const auto &device : found_devices) {
			auto xml_desc = convert_and_free_cstr(virNodeDeviceGetXMLDesc(device.get(), 0));
			auto found_device_id = xml_desc.find("<product id='" + pci_id.device_hex() + "'>");
			auto found_vendor_id = xml_desc.find("<vendor id='" + pci_id.vendor_hex() + "'>");
			if (found_device_id != std::string::npos && found_vendor_id != std::string::npos) {
				auto dev = std::make_shared<Device>(std::move(xml_desc));
				BOOST_LOG_TRIVIAL(trace) << "Adding device: " << dev->address.str();
				devices[host_uri][pci_id].push_back(dev);
			}
		}
	}
	// Copy devices from cache.
	auto vec = devices[host_uri][pci_id];
	BOOST_LOG_TRIVIAL(trace) << "Found " << vec.size() << " devices on cache.";
	// Unlock since no access to devices cache is needed anymore.
	BOOST_LOG_TRIVIAL(trace) << "Unlock since no access to device cache is needed anymore.";
	lock.unlock();
	if (sort_and_shuffle) {
		// Sort potentially attached devices to end of vector.
		BOOST_LOG_TRIVIAL(trace) << "Sort potentially attached devices to end of vector.";
		std::sort(vec.begin(), vec.end(), 
				[](const std::shared_ptr<Device> &lhs, const std::shared_ptr<Device> &rhs)
				{
					return lhs->attached_hint < rhs->attached_hint;
				});
		// Shuffle not attached devices to prevent parallel attachment when starting multiple VMs.
		auto sorted_part_end = std::find_if(vec.begin(), vec.end(),
				[](const std::shared_ptr<Device> &rhs)
				{
					return rhs->attached_hint == true;
				});
		BOOST_LOG_TRIVIAL(trace) << std::count_if(vec.begin(), sorted_part_end, 
						[](const std::shared_ptr<Device>&){return true;})
					 << " devices are marked as being not attached.";
		BOOST_LOG_TRIVIAL(trace) << "Shuffle not attached devices.";
		std::random_device random_seed;
		std::minstd_rand random_generator(random_seed());
		std::shuffle(vec.begin(), sorted_part_end, random_generator);
	}
	return vec;
}

//
// PCI_device_handler implementation
//

PCI_device_handler::PCI_device_handler() :
	device_cache(new Device_cache)
{
}

void PCI_device_handler::attach(virDomainPtr domain, PCI_id pci_id)
{
	// Get connection the domain belongs to.
	BOOST_LOG_TRIVIAL(trace) << "Get connection the domain belongs to.";
	auto connection = virDomainGetConnect(domain);
	// Get vector of devices.
	BOOST_LOG_TRIVIAL(trace) << "Get vector of devices.";
	auto devices = device_cache->get_devices(connection, pci_id);
	if (devices.empty()) {
		throw std::runtime_error("No devices of type \"" + pci_id.str() 
			+ "\" found on \"" + convert_and_free_cstr(virConnectGetURI(connection)) + "\".");
	}
	// Try to attach a device until success or none is left.
	BOOST_LOG_TRIVIAL(trace) << "Try to attach a device until success or none is left.";
	int ret = -1;
	for (const auto &device : devices) {
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

std::unordered_map<PCI_id, size_t> PCI_device_handler::detach(virDomainPtr domain)
{
	// Parse domain xml to get all attached hostdevs.
	BOOST_LOG_TRIVIAL(trace) << "Parse domain xml to get all attached hostdevs.";
	// TODO: Consider reusing hostdev xml descriptions instead of generating later from cached devices.
	auto domain_xml = convert_and_free_cstr(virDomainGetXMLDesc(domain, 0));
	auto domain_ptree = read_xml_from_string(domain_xml);
	auto attached_devices = domain_ptree.get_child("domain.devices");
	BOOST_LOG_TRIVIAL(trace) << "Find attached devices.";
	std::vector<PCI_address> addresses;
	for (const auto &device : attached_devices) {
		if (device.first == "hostdev") {
			addresses.push_back(make_pci_address_from_address_ptree(device.second.get_child("source")));
		}
	}
	BOOST_LOG_TRIVIAL(trace) << "Found " << addresses.size() << " attached devices.";
	// Get PCI-id of devices.
	BOOST_LOG_TRIVIAL(trace) << "Get PCI-id of devices.";
	// TODO: Add method to device cache to find devices 
	auto connection = virDomainGetConnect(domain);	
	std::unordered_map<PCI_id, std::vector<PCI_address>> id_addresses_map;
	for (auto &address : addresses) {
		std::unique_ptr<virNodeDevice, Deleter_virNodeDevice> nodedev;
		nodedev.reset(virNodeDeviceLookupByName(connection, address.to_name_fmt().c_str()));
		auto device_xml = convert_and_free_cstr(virNodeDeviceGetXMLDesc(nodedev.get(), 0));
		auto pci_id = make_pci_id_from_nodedev_xml(device_xml);
		id_addresses_map[pci_id].push_back(std::move(address));
	}
	// Find devices in cache.
	BOOST_LOG_TRIVIAL(trace) << "Find devices in cache.";
	std::vector<std::shared_ptr<Device>> devices;
	for (const auto &id_addresses_pair : id_addresses_map) {
		auto pci_id = id_addresses_pair.first;
		auto addresses = id_addresses_pair.second;
		auto all_devices = device_cache->get_devices(connection, pci_id, false);
		for (const auto &address : addresses) {
			for (const auto &device : all_devices) {
				if (device->address == address) {
					devices.push_back(device);
					break;
				}
			}
		}
	}
	// Detach and reset attached hint.
	BOOST_LOG_TRIVIAL(trace) << "Detach and reset attached hint.";
	for (const auto &device : devices) {
		if (virDomainDetachDevice(domain, device->to_hostdev_xml().c_str()) != 0) {
			auto domain_name = virDomainGetName(domain);
			BOOST_LOG_TRIVIAL(trace) << "Error detaching device " << device->address.str() 
						 << " from " << domain_name << ".";
		}
		device->attached_hint = false;
	}
	// Return detached device types with amount (may help reattaching on dest host)
	std::unordered_map<PCI_id, size_t> types_counts;
	for (const auto &id_addresses_pair : id_addresses_map) {
		types_counts[id_addresses_pair.first] = id_addresses_pair.second.size();
	}

	return types_counts;
}

//
// Migrate_devices_guard implementation
//

Migrate_devices_guard::Migrate_devices_guard(std::shared_ptr<PCI_device_handler> pci_device_handler,
		virDomainPtr domain) :
	pci_device_handler(pci_device_handler),
	domain(domain)
{
	BOOST_LOG_TRIVIAL(trace) << "Detach all devices.";
	detached_types_counts = pci_device_handler->detach(domain);
}

Migrate_devices_guard::~Migrate_devices_guard()
{
	try {
		reattach();
	} catch (...) {
		BOOST_LOG_TRIVIAL(trace) << "Exception while reattaching devices.";
	}
}

void Migrate_devices_guard::set_destination_domain(virDomainPtr dest_domain)
{
	// override domain to reattach devices on
	domain = dest_domain;
}

void Migrate_devices_guard::reattach()
{
	for (auto &type_count : detached_types_counts) {
		for (;type_count.second != 0; --type_count.second) {
			BOOST_LOG_TRIVIAL(trace) << "Reattach device of type " << type_count.first.str();
			pci_device_handler->attach(domain, type_count.first);
		}
	}
}
