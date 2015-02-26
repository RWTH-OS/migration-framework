#include "mqtt_communicator.hpp"
#include "parser.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>

std::string read_file(const std::string &file_name)
{
	std::ifstream file_stream(file_name);
	if (!file_stream.is_open())
		throw std::runtime_error("File " + file_name + " not found.");
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf();
	return string_stream.str();
}

void find_and_replace(std::string &str, const std::string &old_str, const std::string &new_str)
{
	auto pos = str.find(old_str);
	if (pos == std::string::npos)
		throw std::runtime_error("Placeholder not found.");
	str.replace(str.find(old_str), old_str.size(), new_str);
}

int main(int argc, char *argv[])
{
	try {
		unsigned int n;
		std::string vm_name;
		std::string tasks_dir;
		std::string host_name;
		std::string server_a;
		std::string server_b;
		unsigned int memory;
		bool live_migration;
	
		namespace po = boost::program_options;
		po::options_description desc("Options");
		desc.add_options()
			("help,h", "produce help message")
			(",n", po::value<decltype(n)>(&n)->default_value(1), "run benchmark n times")
			("vm-name,V", po::value<std::string>(&vm_name)->required(), "name of virtual machine to use")
			("tasks-dir,t", po::value<std::string>(&tasks_dir)->required(), "path to directory of task files")
			("host-name,H", po::value<std::string>(&host_name)->required(), "name of host to start communicator on")
			("server-a,A", po::value<std::string>(&server_a)->required(), "name of first server")
			("server-b,B", po::value<std::string>(&server_b)->required(), "name of second server")
			("memory,m", po::value<unsigned int>(&memory)->default_value(1024), "memory in MiB to assign to vm")
			("live,l", po::value<bool>(&live_migration)->default_value(false), "live-migration");
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}
		po::notify(vm);
		if (n == 0) {
			std::cout << "n is set to 0 -> exit immediately." << std::endl;
			return EXIT_SUCCESS;
		}
	
		// start communicator
		std::cout << "Starting communicator." << std::endl;
		MQTT_communicator comm("migfra-benchmark", "topic-results", "", host_name, 1883, 60);
	
		// read task strings from files and replace placeholder by arguments
		std::cout << "Reading task strings vom files." << std::endl;
		std::string start_task = read_file(tasks_dir + "/start_task.yaml");
		find_and_replace(start_task, "vm-name-placeholder", vm_name);
		find_and_replace(start_task, "vcpu-placeholder", "1");
		find_and_replace(start_task, "memory-placeholder", std::to_string(memory));
		std::string stop_task = read_file(tasks_dir + "/stop_task.yaml");
		find_and_replace(stop_task, "vm-name-placeholder", vm_name);
		std::string migrate_to_a_task = read_file(tasks_dir + "/migrate_task.yaml");
		find_and_replace(migrate_to_a_task, "vm-name-placeholder", vm_name);
		find_and_replace(migrate_to_a_task, "live-migration-placeholder", live_migration ? "true" : "false");
		std::string migrate_to_b_task = migrate_to_a_task;
		find_and_replace(migrate_to_a_task, "destination-placeholder", server_a);
		find_and_replace(migrate_to_b_task, "destination-placeholder", server_b);
	
		// start vm
		std::cout << "Starting VM." << std::endl;
		bool success = true;
		do {
			try {
				comm.send_message(start_task, "topic-a");
				std::string str = comm.get_message(std::chrono::seconds(1));
				std::cout << str << std::endl;
				auto results = parser::str_to_results(str);

				if (results[0].status != "success")
					throw std::runtime_error("Error while starting vm.");
			} catch (const std::runtime_error &e) {
				if (e.what() == std::string("Timeout while waiting for message.")) {
					std::cout << "Retry starting VM." << std::endl;
					success = false;
				} else throw e;
			}
		} while (!success);
		std::this_thread::sleep_for(std::chrono::seconds(10));

		// migrate (ping pong)
		std::cout << "Starting to migrate." << std::endl;
		std::vector<std::chrono::duration<double, std::nano>> diffs(n);
		//take time
		for (decltype(n) i = 0; i != n; ++i) {
			auto start = std::chrono::high_resolution_clock::now();
			// migrate to b
			comm.send_message(migrate_to_b_task, "topic-a");
			auto results = parser::str_to_results(comm.get_message());
			if (results[0].title == "migrate done" && results[0].status != "success")
				throw std::runtime_error("Migration failed.");

			// migrate to a	
			comm.send_message(migrate_to_a_task, "topic-b");
			results = parser::str_to_results(comm.get_message());
			if (results[0].title == "migrate done" && results[0].status != "success")
				throw std::runtime_error("Migration failed.");

			auto end = std::chrono::high_resolution_clock::now();
			diffs[i] = end - start;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	
		// stop vm
		std::cout << "Stopping VMs." << std::endl;
		comm.send_message(stop_task, "topic-a");
		if (parser::str_to_results(comm.get_message())[0].status != "success")
			throw std::runtime_error("Migration failed.");

		// print results
		std::cout << "Results:" << std::endl;
		std::chrono::duration<double> average_duration(0);
		for (auto &diff : diffs) {
			std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "msec" << std::endl;
			average_duration += diff;
		}
		average_duration /= n;
		std::cout << "Average: ";
		std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(average_duration).count() << "msec" << std::endl;
		return EXIT_SUCCESS;

	} catch (const std::exception &e) {
		std::cout << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
