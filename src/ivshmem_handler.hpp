/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef IVSHMEM_HANDLER_HPP
#define IVSHMEM_HANDLER_HPP

#include <fast-lib/message/migfra/time_measurement.hpp>

#include <libvirt/libvirt.h>
#include <boost/property_tree/ptree.hpp>

#include <string>
#include <vector>
#include <memory>

using Time_measurement = fast::msg::migfra::Time_measurement;

/**
 * \brief A struct representing an ivshmem device.
 */
struct Ivshmem_device
{

	Ivshmem_device(std::string id, std::string size, std::string unit = "M");
	Ivshmem_device(const std::string &xml_desc);

	void from_xml(const std::string &xml_desc);
	std::string to_xml() const;

	std::string id;
	std::string size;
	std::string unit;
	boost::property_tree::ptree pt_pci;
};

/**
 * \brief This function adds the proper xml snippet to the domain config.
 */
void attach_ivshmem_device(virDomainPtr domain, const Ivshmem_device &device);

/**
 * \brief RAII-guard which detaches ivshmem devices in constructor and reattaches in destructor.
 *
 * If no error occures during migration, the destination domain should be set.
 */
class Migrate_ivshmem_guard
{
public:
	Migrate_ivshmem_guard(std::shared_ptr<virDomain> domain, Time_measurement &time_measurement, std::string tag_postfix = "");
	~Migrate_ivshmem_guard() noexcept(false);

	void set_destination_domain(std::shared_ptr<virDomain> dest_domain);
private:
	void detach();
	void reattach();

	std::shared_ptr<virDomain> domain;
	std::vector<Ivshmem_device> detached_devices;
	Time_measurement &time_measurement;
	std::string tag_postfix;
};

#endif
