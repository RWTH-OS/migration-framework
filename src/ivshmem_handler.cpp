#include "ivshmem_handler.hpp"

#include "utility.hpp"
#include "device_utility.hpp"

#include <fast-lib/log.hpp>
#include <libvirt/virterror.h>

#include <regex>
#include <iostream>

FASTLIB_LOG_INIT(ivshmem_handler_log, "Ivshmem_handler")
FASTLIB_LOG_SET_LEVEL_GLOBAL(ivshmem_handler_log, trace);

Ivshmem_device::Ivshmem_device(std::string id, std::string size, std::string unit) :
	id(id),
	size(size),
	unit(unit)
{
}

Ivshmem_device::Ivshmem_device(const std::string &xml_desc)
{
	from_xml(xml_desc);
}

void Ivshmem_device::from_xml(const std::string &xml_desc)
{
	auto pt = read_xml_from_string(xml_desc);
	id = pt.get<decltype(id)>("shmem.<xmlattr>.name");
	size = pt.get<decltype(size)>("shmem.size");
	unit = pt.get<decltype(unit)>("shmem.size.<xmlattr>.unit");
}

std::string Ivshmem_device::to_xml() const
{
/*	static const std::string new_template = "\
<shmem name='@id'>\n\
	<model type='ivshmem-plain'/>\n\
	<size unit='M'>@size</size>\n\
	<alias name='@id'/>\n\
</shmem>\n\
	";
*/
	boost::property_tree::ptree pt;
	pt.put("shmem.<xmlattr>.name", id);
	pt.put("shmem.model.<xmlattr>.type", "ivshmem-plain");
	pt.put("shmem.size.<xmlattr>.unit", unit);
	pt.put("shmem.size", size);
	pt.put("shmem.alias.<xmlattr>.name", id);
	return write_xml_to_string(pt);
}

void attach_ivshmem_device(virDomainPtr domain, const Ivshmem_device &device)
{
	auto ret = virDomainAttachDevice(domain, device.to_xml().c_str());
	if (ret != 0)
		throw std::runtime_error(std::string("Could not attach ivshmem device. ") + virGetLastErrorMessage());
	
}

Migrate_ivshmem_guard::Migrate_ivshmem_guard(std::shared_ptr<virDomain> domain, Time_measurement &time_measurement, std::string tag_postfix) :
	domain(domain),
	time_measurement(time_measurement),
	tag_postfix(std::move(tag_postfix))
{
	if (this->tag_postfix != "")
		this->tag_postfix = "-" + this->tag_postfix;
	FASTLIB_LOG(ivshmem_handler_log, trace) << "Detach all devices.";
	time_measurement.tick("detach-ivshmem-devs" + tag_postfix);
	detach();
	time_measurement.tock("detach-ivshmem-devs" + tag_postfix);
}

Migrate_ivshmem_guard::~Migrate_ivshmem_guard() noexcept(false)
{
	try {
		reattach();
	} catch (...) {
		// Only log exception when unwinding stack, else rethrow exception.
		if (std::uncaught_exception())
			FASTLIB_LOG(ivshmem_handler_log, trace) << "Exception while reattaching ivshmem devices.";
		else
			throw;
	}
}

/// TODO: set settings of destination ivshmem devices
void Migrate_ivshmem_guard::set_destination_domain(std::shared_ptr<virDomain> dest_domain)
{
	domain = dest_domain;
}

void Migrate_ivshmem_guard::detach()
{
	auto domain_ptree = read_xml_from_string(get_domain_xml(domain.get()));
	auto attached_devices = domain_ptree.get_child("domain.devices");
	
	for (const auto &device : attached_devices) {
		if (device.first == "shmem") {
			detached_devices.emplace_back(write_xml_to_string(device.second));
			FASTLIB_LOG(ivshmem_handler_log, trace) << "XML:\n" << write_xml_to_string(device.second);
		}
	}
	if (detached_devices.size() == 0)
		FASTLIB_LOG(ivshmem_handler_log, trace) << "Could not find any attached ivshmem devices.";
	if (detached_devices.size() > 1) 
		throw std::runtime_error("Found more than one ivshmem device. Only migration of one is supported.");
	if (virDomainDetachDevice(domain.get(), detached_devices.front().to_xml().c_str()) != 0) {
		FASTLIB_LOG(ivshmem_handler_log, trace) << "Error detaching device. " << virGetLastErrorMessage();
	}
}

void Migrate_ivshmem_guard::reattach()
{
	time_measurement.tick("reattach-ivshmem-devs" + tag_postfix);
	if (detached_devices.size() != 1)
		FASTLIB_LOG(ivshmem_handler_log, trace) << "Wrong number of detached ivshmem devices: " << detached_devices.size();
	FASTLIB_LOG(ivshmem_handler_log, trace) << "Reattach ivshmem device " << detached_devices.front().to_xml();
	attach_ivshmem_device(domain.get(), detached_devices.front());
	time_measurement.tock("reattach-ivshmem-devs" + tag_postfix);
}
