/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef PARSER_HPP
#define PARSER_HPP

#include "hypervisor.hpp"

#include <fast-lib/communication/communicator.hpp>

#include <memory>
#include <vector>
#include <string>
#include <utility>

/**
 * \brief Namespace for parsing functions.
 *
 * This namespace contains functions to parse and generate yaml strings.
 */
namespace parser
{
	/**
	 * \brief Convert yaml string to Communicator and Hypervisor object.
	 *
	 * Parses a yaml string (e.g. the config file) and generates a Communicator and Hypervisor.
	 */
	std::pair<std::shared_ptr<fast::Communicator>, std::shared_ptr<Hypervisor>> parse_config(const std::string &str);
};


#endif
