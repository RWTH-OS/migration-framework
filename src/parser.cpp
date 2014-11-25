#include "parser.hpp"

#include "task.hpp"

#include <yaml-cpp/yaml.h>

#include <stdexcept>

std::unique_ptr<Task> Parser::str_to_task(const std::string &str)
{
	std::unique_ptr<Task> task;
	if (str == "start vm")
		task = std::unique_ptr<Task>(new Start("vm1", 2, 1024));
	else if (str == "stop vm")
		task = std::unique_ptr<Task>(new Stop("vm1"));
	else if (str == "migrate start")
		task = std::unique_ptr<Task>(new Migrate("vm1", "pandora2", false));
	else
		throw std::invalid_argument("Unknown Task");
	return task;
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
