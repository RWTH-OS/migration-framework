/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task_handler.hpp"
#include "logging.hpp"
#include "conf.hpp"

#include <boost/program_options.hpp>

#include <unistd.h>
#include <cstdlib>
#include <string>
#include <exception>
#include <iostream>

int main(int argc, char *argv[])
{
	try {
		namespace po = boost::program_options;
		po::options_description desc("Options");
		desc.add_options()
			("help", "produce help message")
			("config", po::value<std::string>(), "path to config file");
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}
		std::string config_file_name = "migfra.conf";
		if (vm.count("config"))
			config_file_name = vm["config"].as<std::string>();
		Task_handler task_handler(config_file_name);
		LOG_PRINT(LOG_DEBUG, "task_handler loop started.");
		task_handler.loop();
		LOG_PRINT(LOG_DEBUG, "task_handler loop closed.");
	} catch (const std::exception &e) {
		LOG_STREAM(LOG_ERR, "Exception: " << e.what());
	}
	return EXIT_SUCCESS;
}
