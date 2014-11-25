#ifndef PARSER_HPP
#define PARSER_HPP

#include "task.hpp"

#include <memory>
#include <vector>
#include <string>

class Parser
{
public:
	std::unique_ptr<Task> str_to_task(const std::string &str);
	std::string results_to_str(const std::vector<Result> &result);
private:
};

#endif
