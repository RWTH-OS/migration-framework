/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef TASK_HANDLER_HPP
#define TASK_HANDLER_HPP

#include "hypervisor.hpp"

#include <fast-lib/communication/communicator.hpp>
#include <fast-lib/serialization/serializable.hpp>

#include <memory>
#include <string>

/**
 * \brief Class to handle incoming tasks.
 *
 * The loop() first waits for a message from comm, parses the message gernerating a task using the parser.
 * This task is then executed using the hypervisor.
 * The specialized type of Hypervisor and Communicator are defined in a config file which is parsed on construction
 * of the Task_handler.
 */
class Task_handler : fast::Serializable
{
public:
	/**
	 * \brief Construct a Task_handler.
	 *
	 * Parses the config file using the parser and constructs specialized Communicator and Hypervisor.
	 * Config file must be in YAML.
	 * \param config_file The name of the config file to parse.
	 */
	Task_handler(const std::string &config_file);
	/**
	 * \brief Destruct Task_handler.
	 *
	 * The destructor waits for all threads to finish.
	 */
	~Task_handler();
	/**
	 * \brief Starts the main loop.
	 *
	 * This loop receives messages from the Communicator, parses them using the \link parser \endlink  and executes 
	 * generated Task by using the Hypervisor.
	 */
	void loop();
	/**
	 * \brief Emits Task_handler to YAML::Node.
	 *
	 * Implements fast::Serializable::emit().
	 * Dummy implementation which always throws, due to lack of need for this method.
	 */
	YAML::Node emit() const override;
	/**
	 * \brief Loads Task_handler from YAML::Node.
	 *
	 * Implements fast::Serializable::load().
	 * Creates the Communicator and Hypervisor from YAML.
	 */
	void load(const YAML::Node &node) override;
private:
	std::shared_ptr<fast::Communicator> comm;
	std::shared_ptr<Hypervisor> hypervisor;
	bool running;
};

#endif
