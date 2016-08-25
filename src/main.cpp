/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task_handler.hpp"

#include "utility.hpp"

#include <boost/program_options.hpp>
#include <mosquittopp.h>

#include <unistd.h>
#include <cstdlib>
#include <string>
#include <exception>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>


int main(int argc, char *argv[])
{
	try {
		mosqpp::lib_init();
		namespace po = boost::program_options;
		po::options_description desc("Options");
		desc.add_options()
			("help,h", "produce help message")
			("config,c", po::value<std::string>(), "path to config file")
			("daemon,d", "start as daemon")
			("log,l", po::value<std::string>(), "path to log file");
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
		config_file_name = convert_and_free_cstr(realpath(config_file_name.c_str(), nullptr));
		if (vm.count("log")) {
			auto log_file_name = vm["log"].as<std::string>();
			auto log_file = fopen(log_file_name.c_str(), "a");
			dup2(fileno(log_file), STDIN_FILENO);
			dup2(fileno(log_file), STDOUT_FILENO);
			dup2(fileno(log_file), STDERR_FILENO);
			fclose(log_file);
		}
		if (vm.count("daemon")) {
			std::cout << "Starting migfra daemon." << std::endl;
			pid_t pid;
			pid_t sid;
			pid = fork();
			if (pid < 0)
				return EXIT_FAILURE;
			if (pid > 0)
				exit(EXIT_SUCCESS);
			umask(0);
			sid = setsid();
			if (sid < 0)
				return EXIT_FAILURE;
			if (chdir("/") < 0)
				return EXIT_FAILURE;
			//TODO redirect stdout to syslog
			if (!vm.count("log")) {
				close(STDIN_FILENO);
				close(STDOUT_FILENO);
				close(STDERR_FILENO);
			}
		}
		Task_handler task_handler(config_file_name);
		std::cout << "Debug: task_handler loop started." << std::endl;
		task_handler.loop();
		std::cout << "Debug: task_handler loop closed." << std::endl;
	} catch (const std::exception &e) {
		std::cout << "Exception: " << e.what() << std::endl;
	}
	mosqpp::lib_cleanup();
	return EXIT_SUCCESS;
}
