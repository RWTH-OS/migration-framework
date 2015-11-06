/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task_handler.hpp"

#include "libvirt_hypervisor.hpp"
#include "dummy_hypervisor.hpp"
#include "task.hpp"

#include <fast-lib/communication/mqtt_communicator.hpp>
#include <mosquittopp.h>
#include <boost/regex.hpp>

#include <unistd.h>
#include <cstring>
#include <climits>
#include <exception>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>

Task_handler::Task_handler(const std::string &config_file) : 
	running(true)
{
	// Convert config file to string
	std::ifstream file_stream(config_file);
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf(); // Filestream to stingstream conversion
	auto config = string_stream.str();
	// Get hostname
	char hostname_cstr[HOST_NAME_MAX];
	int ret;
	if ((ret = gethostname(hostname_cstr, HOST_NAME_MAX)) != 0)
		std::runtime_error(std::string("Failed getting hostname: ") + std::strerror(ret));
	const std::string hostname(hostname_cstr, std::strlen(hostname_cstr));
	// Replace placeholder for hostname in config
	boost::regex hostname_regex("(<hostname>)");
	config = boost::regex_replace(config, hostname_regex, hostname);
	// Load config from string
	from_string(config);
}

Task_handler::~Task_handler()
{
	Thread_counter::wait_for_threads_to_finish();
}


void Task_handler::loop()
{
	while (running) {
		std::string msg;
		try {
			msg = comm->get_message();
			Task task;	
			task.from_string(msg);
			task.execute(hypervisor, comm);
		} catch (const YAML::Exception &e) {
			std::cout << "Exception while parsing message." << std::endl;
			std::cout << e.what() << std::endl;
			std::cout << "msg dump: " << msg << std::endl;
		} catch (const Task::no_task_exception &e) {
			std::cout << "Debug: Parsed message not being a Task." << std::endl;
		} catch (const std::exception &e) {
			if (e.what() == std::string("quit")) {
				running = false;
				std::cout << "Debug: Quit msg received." << std::endl;
			} else {
				std::cout << "Exception: " << e.what() << std::endl;
				std::cout << "msg dump: " << msg << std::endl;
			}
		}
	}
}

YAML::Node Task_handler::emit() const
{
	throw std::runtime_error("Task_handler::emit() is not implemented.");
	YAML::Node node;
	return node;
}

void Task_handler::load(const YAML::Node &node)
{
	if (!node["communicator"])
		throw std::invalid_argument("No configuration for communication interface.");
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
	{
		auto hypervisor_node = node["hypervisor"];
		if (!hypervisor_node["type"])
			throw std::invalid_argument("No type for hypervisor interface in configuration found.");
		auto type = hypervisor_node["type"].as<std::string>();
		if (type == "libvirt") {
			hypervisor = std::make_shared<Libvirt_hypervisor>();
		} else if (type == "dummy") {
			if (!hypervisor_node["never-throw"])
				throw std::invalid_argument("Defective configuration for dummy hypervisor.");
			hypervisor = std::make_shared<Dummy_hypervisor>(hypervisor_node["never-throw"].as<bool>());
		} else {
			throw std::invalid_argument("Unknown communcation type in configuration found");
		}
	}
}

