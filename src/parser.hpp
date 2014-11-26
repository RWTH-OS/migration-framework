#ifndef PARSER_HPP
#define PARSER_HPP

#include "task.hpp"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <vector>
#include <string>

class Parser
{
public:
	std::unique_ptr<Task> str_to_task(const std::string &str);
	std::string results_to_str(const std::vector<Result> &result);
private:
	std::unique_ptr<Task> generate_start_task(const YAML::Node &node);
	std::unique_ptr<Task> generate_stop_task(const YAML::Node &node);
	std::unique_ptr<Task> generate_migrate_task(const YAML::Node &node);
};

#endif
