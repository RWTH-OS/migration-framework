/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "parser.hpp"

#include <fast-lib/communication/mqtt_communicator.hpp>
#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <iostream>

namespace parser {

std::shared_ptr<fast::Communicator> str_to_communicator(const std::string &str)
{
	YAML::Node node = YAML::Load(str);
	if (!node["id"] || !node["subscribe-topic"] || !node["publish-topic"] 
			|| !node["host"] || !node["port"] || !node["keepalive"])
		throw std::invalid_argument("Defective configuration for communicator.");
	return std::make_shared<fast::MQTT_communicator>(
			node["id"].as<std::string>(),
			node["subscribe-topic"].as<std::string>(),
			node["publish-topic"].as<std::string>(),
			node["host"].as<std::string>(),
			node["port"].as<int>(),
			node["keepalive"].as<int>());
}

} // namespace parser
