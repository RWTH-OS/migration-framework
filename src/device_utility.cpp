/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "device_utility.hpp"

#include <boost/property_tree/xml_parser.hpp>

#include <stdexcept>
#include <sstream>

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

boost::property_tree::ptree read_xml_from_string(const std::string &str)
{
	boost::property_tree::ptree pt;
	std::stringstream ss(str);
	read_xml(ss, pt, boost::property_tree::xml_parser::trim_whitespace);
	return pt;
}

std::string write_xml_to_string(const boost::property_tree::ptree &ptree, bool pretty)
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
