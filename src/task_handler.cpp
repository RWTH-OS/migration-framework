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
#include "pscom_handler.hpp"
#include "utility.hpp"

#include <fast-lib/mqtt_communicator.hpp>
#include <fast-lib/log.hpp>
#include <mosquittopp.h>
#include <boost/regex.hpp>

#include <exception>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>

FASTLIB_LOG_INIT(migfra_task_handler_log, "Task_handler")
FASTLIB_LOG_SET_LEVEL_GLOBAL(migfra_task_handler_log, trace);

using Task_container = fast::msg::migfra::Task_container;

Task_handler::Task_handler(const std::string &config_file) : 
	running(true)
{
	// Convert config file to string
	std::ifstream file_stream(config_file);
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf(); // Filestream to stingstream conversion
	auto config = string_stream.str();
	const std::string hostname = get_hostname();
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
			Task_container task_cont;	
			task_cont.from_string(msg);
			execute(task_cont, hypervisor, comm);
		} catch (const YAML::Exception &e) {
			send_parse_error_nothrow(comm, std::string("Exception while parsing message: ") + e.what());
			FASTLIB_LOG(migfra_task_handler_log, trace) << "msg dump: " << msg;
		} catch (const Task_container::no_task_exception &e) {
			send_parse_error_nothrow(comm, "Parsed message not being a Task_container.");
		} catch (const std::exception &e) {
			if (e.what() == std::string("quit")) {
				running = false;
				FASTLIB_LOG(migfra_task_handler_log, trace) << "Quit msg received.";
			} else {
				send_parse_error_nothrow(comm, std::string("Exception: ") + e.what());
				FASTLIB_LOG(migfra_task_handler_log, trace) << "msg dump: " << msg;
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
			std::vector<std::string> nodes;
			if (hypervisor_node["nodes"])
				nodes = hypervisor_node["nodes"].as<decltype(nodes)>();
			std::string default_driver = "qemu";
			if (hypervisor_node["driver"])
				default_driver = hypervisor_node["driver"].as<decltype(default_driver)>();
			std::string default_transport = "ssh";
			if (hypervisor_node["transport"])
				default_transport = hypervisor_node["transport"].as<decltype(default_transport)>();
			hypervisor = std::make_shared<Libvirt_hypervisor>(std::move(nodes), default_driver, default_transport);
		} else if (type == "dummy") {
			if (!hypervisor_node["never-throw"])
				throw std::invalid_argument("Defective configuration for dummy hypervisor.");
			hypervisor = std::make_shared<Dummy_hypervisor>(hypervisor_node["never-throw"].as<bool>());
		} else {
			throw std::invalid_argument("Unknown communcation type in configuration found");
		}
	}
	if (node["pscom-handler"]) {
		auto pscom_node = node["pscom-handler"];
		if (pscom_node["request-topic"])
			Pscom_handler::set_request_topic_template(pscom_node["request-topic"].as<std::string>());
		if (pscom_node["response-topic"])
			Pscom_handler::set_response_topic_template(pscom_node["response-topic"].as<std::string>());
		if (pscom_node["qos"])
			Pscom_handler::set_qos(pscom_node["qos"].as<int>());
	}
}

