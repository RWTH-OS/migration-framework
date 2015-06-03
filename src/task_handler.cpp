/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task_handler.hpp"

#include "mqtt_communicator.hpp"
#include "libvirt_hypervisor.hpp"
#include "parser.hpp"
#include "task.hpp"

#include <mosquittopp.h>

#include <string>
#include <exception>
#include <fstream>
#include <sstream>
#include <iostream>

Task_handler::Task_handler(const std::string &config_file) : 
	hypervisor(std::make_shared<Libvirt_hypervisor>()),
	running(true)
{
	std::ifstream file_stream(config_file);
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf(); // Filestream to stingstream conversion
	comm = parser::str_to_communicator(string_stream.str());
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
