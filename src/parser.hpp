#ifndef PARSER_HPP
#define PARSER_HPP

#include "task.hpp"

#include <yaml-cpp/yaml.h>

#include <memory>
#include <vector>
#include <string>

/**
 * \brief Namespace for parsing function.
 *
 * This namespace contains functions to parse and generate yaml strings.
 */
namespace parser
{
	/**
	 * \brief Convert yaml string to task object.
	 *
	 * Parses the yaml string and generates a task object.
	 */
	std::unique_ptr<Task> str_to_task(const std::string &str);

	/**
	 * \brief Convert results to yaml string.
	 *
	 * Generates yaml string using result object.
	 */
	std::string results_to_str(const std::vector<Result> &result);
};

#endif
