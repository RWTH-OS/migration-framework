#include "parser.hpp"

#include "task.hpp"
#include "mqtt_communicator.hpp"
#include "logging.hpp"

#include <stdexcept>
#include <iostream>

namespace parser {

Task generate_start_task(const YAML::Node &node)
{
	if (!node["vm-configurations"])
		throw std::invalid_argument("No vm-configurations for start_task found.");
	std::vector<std::shared_ptr<Sub_task>> start_tasks;
	for (auto &iter : node["vm-configurations"]) {
		if (!iter["name"] || !iter["vcpus"] || !iter["memory"])
			throw std::invalid_argument("Invalid vm-config in start_task found.");
		auto name = iter["name"].as<std::string>();
		auto vcpus = iter["vcpus"].as<unsigned int>();
		auto memory = iter["memory"].as<unsigned long>();
		auto concurrent_execution = iter["concurrent-execution"].as<bool>(true);
		start_tasks.push_back(std::make_shared<Start>(name, vcpus, memory, concurrent_execution));
	}
	auto concurrent_execution = node["concurrent-execution"].as<bool>(true);
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
		auto vm_name = iter["vm-name"].as<std::string>();
		auto concurrent_execution = iter["concurrent-execution"].as<bool>(true);
		stop_tasks.push_back(std::make_shared<Stop>(vm_name, concurrent_execution));
	}
	auto concurrent_execution = node["concurrent-execution"].as<bool>(true);
	return Task(std::move(stop_tasks), concurrent_execution);
}

Task generate_migrate_task(const YAML::Node &node)
{
	if (!node["vm-name"] || !node["destination"] || !node["parameter"] || !node["parameter"]["live-migration"])
		throw std::invalid_argument("Invalid vm-config in migrate_task found.");
	auto vm_name = node["vm-name"].as<std::string>();
	auto destination = node["destination"].as<std::string>();
	auto live_migration = node["parameter"]["live-migration"].as<bool>();
	auto concurrent_execution = node["concurrent-execution"].as<bool>(true);
	std::vector<std::shared_ptr<Sub_task>> migrate_tasks;
	migrate_tasks.push_back(std::make_shared<Migrate>(vm_name, destination, live_migration, false));
	return Task(std::move(migrate_tasks), concurrent_execution);
}

Task str_to_task(const std::string &str)
{
	YAML::Node node = YAML::Load(str);
	if (node["task"]) {
		LOG_PRINT(LOG_DEBUG, "Parsing task...");
		auto task_name = node["task"].as<std::string>();
		if (task_name == "start vm")
			return generate_start_task(node);
		else if (task_name == "stop vm")
			return generate_stop_task(node);
		else if (task_name == "migrate start")
			return generate_migrate_task(node);
		else if (task_name == "quit")
			throw std::runtime_error("quit");
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
		if (iter.details != "")
			sub_node["details"] = iter.details;
		node["list"].push_back(sub_node);
	}
	return "---\n" + YAML::Dump(node) + "\n...";
}

std::vector<Result> str_to_results(const std::string &str)
{
	YAML::Node node = YAML::Load(str);
	if (!node["result"] || !node["list"])
		throw std::invalid_argument("Invalid result string.");
	std::vector<Result> results;
	for (const auto &iter : node["list"]) {
		if (!iter["vm-name"] || !iter["status"])
			throw std::invalid_argument("Invalid result list.");
		results.emplace_back(node["result"].as<std::string>(),
				iter["vm-name"].as<std::string>(),
				iter["status"].as<std::string>(),
				iter["details"] ? iter["details"].as<std::string>() : "");
	}
	return results;
}


std::shared_ptr<Communicator> str_to_communicator(const std::string &str)
{
	YAML::Node node = YAML::Load(str);
	if (!node["id"] || !node["subscribe-topic"] || !node["publish-topic"] 
			|| !node["host"] || !node["port"] || !node["keepalive"])
		throw std::invalid_argument("Defective configuration for communicator.");
	return std::make_shared<MQTT_communicator>(
			node["id"].as<std::string>(),
			node["subscribe-topic"].as<std::string>(),
			node["publish-topic"].as<std::string>(),
			node["host"].as<std::string>(),
			node["port"].as<int>(),
			node["keepalive"].as<int>());
}

} // namespace parser
