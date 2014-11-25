#include "parser.hpp"

#include "task.hpp"

#include <yaml-cpp/yaml.h>

#include <stdexcept>

std::unique_ptr<Task> Parser::str_to_task(const std::string &str)
{
	std::unique_ptr<Task> task;
	if (str == "start")
		task = std::unique_ptr<Task>(new Start("vm1", 2, 1024));
	else if (str == "stop")
		task = std::unique_ptr<Task>(new Stop("vm1"));
	else if (str == "migrate")
		task = std::unique_ptr<Task>(new Migrate("vm1", "pandora2", false));
	else
		throw std::invalid_argument("Unknown Task");
	return task;
}
