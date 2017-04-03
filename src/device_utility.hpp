/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef DEVICE_UTILITY_HPP
#define DEVICE_UTILITY_HPP

#include <libvirt/libvirt.h>
#include <boost/property_tree/ptree.hpp>

#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

// Deleter to be used with smart pointers.
struct Deleter_virNodeDevice
{
	void operator()(virNodeDevicePtr ptr) const
	{
		virNodeDeviceFree(ptr);
	}
};

// Wraps ugly passing of C style array of raw pointers to returning vector of smart pointers.
std::vector<std::unique_ptr<virNodeDevice, Deleter_virNodeDevice>> list_all_node_devices_wrapper(virConnectPtr conn, unsigned int flags);

// Converts integer type numbers to string in hex format.
template<typename T, typename std::enable_if<std::is_integral<T>{}>::type* = nullptr> 
std::string to_hex_string(const T &integer, int digits, bool show_base = true)
{
	std::stringstream ss;
	ss << (show_base ? "0x" : "") << std::hex << std::setfill('0') << std::setw(digits) << +integer;
	return ss.str();
}

// Convert xml string to ptree.
boost::property_tree::ptree read_xml_from_string(const std::string &str);

// Convert ptree to xml string.
std::string write_xml_to_string(const boost::property_tree::ptree &ptree, bool pretty = true);

#endif
