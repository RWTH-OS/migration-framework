#include "parser.hpp"

#include "task.hpp"
#include "mqtt_communicator.hpp"
#include "logging.hpp"

#include <stdexcept>

namespace parser {

Task generate_start_task(const YAML::Node &node)
{
	if (!node["vm-configurations"])
		throw std::invalid_argument("No vm-configurations for start_task found.");
	std::vector<std::shared_ptr<Sub_task>> start_tasks;
	for (auto &iter : node["vm-configurations"]) {
		if (!iter["name"] || !iter["vcpus"] || !iter["memory"])
			throw std::invalid_argument("Invalid vm-config in start_task found.");
		std::string name = iter["name"].as<std::string>();
		size_t vcpus = iter["vcpus"].as<size_t>();
		size_t memory = iter["memory"].as<size_t>();
		bool concurrent_execution = iter["concurrent_execution"] ? 
			iter["concurrent_execution"].as<bool>() : true; // concurrent execution is default.
		start_tasks.emplace_back(new Start(name, vcpus, memory, concurrent_execution));
	}
	bool concurrent_execution = node["concurrent_execution"] ? 
		node["concurrent_execution"].as<bool>() : true; // concurrent execution is default.
	return Task(std::move(start_tasks), concurrent_execution);
}

Task generate_stop_task(const YAML::Node &node)
{
	if (!node["vm-configurations"])
		throw std::invalid_argument("No vm-configurations for stop_task found.");
	std::vector<std::shared_ptr<Sub_task>> stop_tasks;
	for (auto &iter : node["vm-configurations"]) {
		if (!iter["vm-name"])
			throw std::invalid_argument("Invalid vm-config in stop_task found.");
		std::string vm_name = iter["vm-name"].as<std::string>();
		bool concurrent_execution = iter["concurrent_execution"] ? 
			iter["concurrent_execution"].as<bool>() : true; // concurrent execution is default.
		stop_tasks.emplace_back(new Stop(vm_name, concurrent_execution));
	}
	bool concurrent_execution = node["concurrent_execution"] ? 
		node["concurrent_execution"].as<bool>() : true; // concurrent execution is default.
	return Task(std::move(stop_tasks), concurrent_execution);
}

Task generate_migrate_task(const YAML::Node &node)
{
	if (!node["vm-name"] || !node["destination"] || !node["parameter"] || !node["parameter"]["live-migration"])
		throw std::invalid_argument("Invalid vm-config in migrate_task found.");
	std::string vm_name = node["vm-name"].as<std::string>();
	std::string destination = node["destination"].as<std::string>();
	bool live_migration = node["parameter"]["live-migration"].as<bool>();
	bool concurrent_execution = node["concurrent_execution"] ? 
		node["concurrent_execution"].as<bool>() : true; // concurrent execution is default.
	std::vector<std::shared_ptr<Sub_task>> migrate_tasks;
	migrate_tasks.emplace_back(new Migrate(vm_name, destination, live_migration, false));
	return Task(std::move(migrate_tasks), concurrent_execution);
}

Task str_to_task(const std::string &str)
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
		return Task();
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


std::shared_ptr<Communicator> str_to_communicator(const std::string &str)
{
	YAML::Node node = YAML::Load(str);
	if (!node["id"] || !node["topic"] || !node["host"] || !node["port"] || !node["keepalive"])
		throw std::invalid_argument("Defective configuration for communicator.");
	return std::make_shared<MQTT_communicator>(
			node["id"].as<std::string>(),
			node["topic"].as<std::string>(),
			node["host"].as<std::string>(),
			node["port"].as<int>(),
			node["keepalive"].as<int>());
}

} // namespace parser
