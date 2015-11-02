/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef PCI_DEVICE_HANDLER_HPP
#define PCI_DEVICE_HANDLER_HPP

#include <fast-lib/serialization/serializable.hpp>

#include <libvirt/libvirt.h>
#include <boost/property_tree/ptree.hpp>

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

// Contains the vendor id and device id to identify a PCI device type.
struct PCI_id :
	public fast::Serializable
{
	using vendor_t = unsigned short;
	using device_t = unsigned short;

	PCI_id(vendor_t vendor, device_t device);
	// Default (copy-)constructor and assignment for use in std::vector.
	PCI_id() = default;
	PCI_id(const PCI_id &) = default;
	PCI_id & operator=(const PCI_id &) = default;

	bool operator==(const PCI_id &rhs) const;
	std::string vendor_hex() const;
	std::string device_hex() const;
	std::string str() const;

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;

	vendor_t vendor;
	device_t  device;
};
YAML_CONVERT_IMPL(PCI_id)

// std::hash specialization to make PCI_id a valid key for unordered_map.
namespace std
{
	template<> struct hash<PCI_id>
	{
		typedef PCI_id argument_type;
		typedef std::size_t result_type;
		result_type operator()(const argument_type &s) const
		{
			const result_type h1(std::hash<PCI_id::vendor_t>()(s.vendor));
			const result_type h2(std::hash<PCI_id::device_t>()(s.device));
			return h1 ^ (h2 << 1);
		}
	};
}

// Contains pci address and methods to convert from and to ptree.
struct PCI_address
{
	using domain_t = unsigned short;
	using bus_t = unsigned char;
	using slot_t = unsigned char;
	using function_t = unsigned char;

	PCI_address(domain_t domain, bus_t bus, slot_t slot, function_t function);

	bool operator==(const PCI_address &rhs) const;
	boost::property_tree::ptree to_address_ptree() const;
	std::string str() const;
	std::string to_name_fmt() const;

	const domain_t domain;
	const bus_t bus;
	const slot_t slot;
	const function_t function;
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
	std::vector<std::shared_ptr<Device>> get_devices(virConnectPtr host_connection, PCI_id pci_id, bool sort_and_shuffle = true) const;
private:
	// (hosturi : (pci_id : xml_desc))
	mutable std::unordered_map<std::string, std::unordered_map<PCI_id, std::vector<std::shared_ptr<Device>>>> devices;
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
	void attach(virDomainPtr domain, PCI_id pci_id);
	/**
	 * \brief Detach device of certain type to domain.
	 *
	 * \returns A map with type id as key and the number of detached devices of that type as value.
	 */
	std::unordered_map<PCI_id, size_t> detach(virDomainPtr domain);
private:
	std::unique_ptr<const Device_cache> device_cache;	
};

// RAII-guard to detach devices in constructor and reattach in destructor.
// If no error occures during migration the domain on destination should be set.
class Migrate_devices_guard
{
public:
	Migrate_devices_guard(std::shared_ptr<PCI_device_handler> pci_device_handler, virDomainPtr domain);
	~Migrate_devices_guard();
	void reattach_on_destination(virDomainPtr);
	void reattach();
private:
	std::shared_ptr<PCI_device_handler> pci_device_handler;
	virDomainPtr domain;
	std::unordered_map<PCI_id, size_t> detached_types_counts;
};

#endif
