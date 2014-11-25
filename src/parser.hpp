#ifndef PARSER_HPP
#define PARSER_HPP

#include "task.hpp"

#include <memory>

class Parser
{
public:
	std::unique_ptr<Task> str_to_task(const std::string &str);
private:
};

#endif
