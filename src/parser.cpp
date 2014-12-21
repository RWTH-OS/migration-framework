#include "parser.hpp"

#include "task.hpp"
#include "logging.hpp"

#include <stdexcept>

namespace parser {

std::unique_ptr<Task> generate_start_task(const YAML::Node &node)
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

std::unique_ptr<Task> generate_stop_task(const YAML::Node &node)
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

std::unique_ptr<Task> generate_migrate_task(const YAML::Node &node)
{
	if (!node["vm-name"] || !node["destination"] || !node["parameter"] || !node["parameter"]["live-migration"])
		throw std::invalid_argument("Invalid vm-config in migrate_task found.");
	std::string vm_name = node["vm-name"].as<std::string>();
	std::string destination = node["destination"].as<std::string>();
	bool live_migration = node["parameter"]["live-migration"].as<bool>();
	return std::unique_ptr<Task>(new Migrate(vm_name, destination, live_migration));
}

std::unique_ptr<Task> str_to_task(const std::string &str)
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

std::string results_to_str(const std::vector<Result> &results)
{
	YAML::Node node;
	if (results.empty())
		throw std::invalid_argument("No results to parse.");
	node["result"] = results[0].title;
	for (auto &iter : results) {
		YAML::Node sub_node;
		sub_node["vm-name"] = iter.vm_name;
		sub_node["status"] = iter.status;
		node["list"].push_back(sub_node);
	}
	return "---\n" + YAML::Dump(node) + "\n...";
}

} // namespace parser
