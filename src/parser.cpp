#include "parser.hpp"

#include "task.hpp"
#include "logging.hpp"

#include <stdexcept>

std::unique_ptr<Task> Parser::str_to_task(const std::string &str)
{
	YAML::Node node = YAML::Load(str);
	if (node["task"]) {
		LOG_PRINT(LOG_DEBUG, "Parsing task...");
		std::string task_name = node["task"].as<std::string>();
		if (task_name == "start vm")
			return generate_start_task(node);
		else if (task_name == "stop vm")
			return generate_stop_task(node);
		else if (task_name == "migrate start")
			return generate_migrate_task(node);
		else
			throw std::invalid_argument("Unknown task_name.");
	} else if (node["result"]) {
		LOG_PRINT(LOG_DEBUG, "Parsing result...");
		return std::unique_ptr<Task>();
	} else {
		throw std::invalid_argument("Unknown message type.");
	}
}

std::unique_ptr<Task> Parser::generate_start_task(const YAML::Node &node)
{
	if (!node["vm-configurations"])
		throw std::invalid_argument("No vm-configurations for start_task found.");
	std::vector<Start> start_tasks;
	for (auto &iter : node["vm-configurations"]) {
		if (!iter["name"] || !iter["vcpus"] || !iter["memory"])
			throw std::invalid_argument("Invalid vm-config in start_task found.");
		std::string name = iter["name"].as<std::string>();
		size_t vcpus = iter["vcpus"].as<size_t>();
		size_t memory = iter["memory"].as<size_t>();
		start_tasks.push_back(Start(name, vcpus, memory));
	}
	return std::unique_ptr<Task>(new Start_packed(start_tasks));
}

std::unique_ptr<Task> Parser::generate_stop_task(const YAML::Node &node)
{
	if (!node["vm-configurations"])
		throw std::invalid_argument("No vm-configurations for stop_task found.");
	std::vector<Stop> stop_tasks;
	for (auto &iter : node["vm-configurations"]) {
		if (!iter["vm-name"])
			throw std::invalid_argument("Invalid vm-config in stop_task found.");
		std::string vm_name = iter["vm-name"].as<std::string>();
		stop_tasks.push_back(Stop(vm_name));
	}
	return std::unique_ptr<Task>(new Stop_packed(stop_tasks));
}

std::unique_ptr<Task> Parser::generate_migrate_task(const YAML::Node &node)
{
	if (!node["vm-name"] || !node["destination"] || !node["parameter"] || !node["parameter"]["live-migration"])
		throw std::invalid_argument("Invalid vm-config in migrate_task found.");
	std::string vm_name = node["vm-name"].as<std::string>();
	std::string destination = node["destination"].as<std::string>();
	bool live_migration = node["parameter"]["live-migration"].as<bool>();
	return std::unique_ptr<Task>(new Migrate(vm_name, destination, live_migration));
}

std::string Parser::results_to_str(const std::vector<Result> &results)
{
	std::string str;
	if (results[0].title == "vm started")
		str = results[0].title;
	else if (results[0].title == "vm stopped")
		str = results[0].title;
	else if (results[0].title == "migrate done")
		str = results[0].title;
	else
		throw std::invalid_argument("Unknown Result");
	return str;
}

