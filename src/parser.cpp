/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "parser.hpp"

#include "libvirt_hypervisor.hpp"
#include "dummy_hypervisor.hpp"

#include <fast-lib/communication/mqtt_communicator.hpp>
#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <iostream>

namespace parser {

std::pair<std::shared_ptr<fast::Communicator>, std::shared_ptr<Hypervisor>> parse_config(const std::string &str)
{
	
	YAML::Node node = YAML::Load(str);
	if (!node["communicator"])
		throw std::invalid_argument("No configuration for communication interface.");
	std::shared_ptr<fast::Communicator> comm;
	{
		auto comm_node = node["communicator"];
		if (!comm_node["type"])
			throw std::invalid_argument("No type for communication interface in configuration found.");
		auto type = comm_node["type"].as<std::string>();
		if (type == "mqtt") {
			if (!comm_node["id"] || !comm_node["subscribe-topic"] || !comm_node["publish-topic"] 
					|| !comm_node["host"] || !comm_node["port"] || !comm_node["keepalive"])
				throw std::invalid_argument("Defective configuration for mqtt communicator.");
			comm = std::make_shared<fast::MQTT_communicator>(
				comm_node["id"].as<std::string>(),
				comm_node["subscribe-topic"].as<std::string>(),
				comm_node["publish-topic"].as<std::string>(),
				comm_node["host"].as<std::string>(),
				comm_node["port"].as<int>(),
				comm_node["keepalive"].as<int>());
		} else {
			throw std::invalid_argument("Unknown communcation type in configuration found");
		}
	}
	if (!node["hypervisor"])
		throw std::invalid_argument("No configuration for hypervisor interface.");
	std::shared_ptr<Hypervisor> hypervisor;
	{
		auto hypervisor_node = node["hypervisor"];
		if (!hypervisor_node["type"])
			throw std::invalid_argument("No type for hypervisor interface in configuration found.");
		auto type = hypervisor_node["type"].as<std::string>();
		if (type == "libvirt") {
			hypervisor = std::make_shared<Libvirt_hypervisor>();
		} else if (type == "dummy") {
			hypervisor = std::make_shared<Dummy_hypervisor>();
		} else {
			throw std::invalid_argument("Unknown communcation type in configuration found");
		}
	}
	return std::make_pair(comm, hypervisor);
}

} // namespace parser
