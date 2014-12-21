#ifndef PARSER_HPP
#define PARSER_HPP

#include "task.hpp"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <vector>
#include <string>

namespace parser
{
	std::unique_ptr<Task> str_to_task(const std::string &str);
	std::string results_to_str(const std::vector<Result> &result);
};

#endif
