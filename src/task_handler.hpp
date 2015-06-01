/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef TASK_HANDLER_HPP
#define TASK_HANDLER_HPP

#include <memory>

#include "communicator.hpp"
#include "hypervisor.hpp"

/**
 * \brief Class to handle incoming tasks.
 *
 * The loop() first waits for a message from comm, parses the message gernerating a task using the parser.
 * This task is then executed using the hypervisor.
 * The specialized type of Hypervisor and Communicator are defined in a config file which is parsed on construction
 * of the Task_handler.
 */
class Task_handler
{
public:
	/**
	 * \brief Construct a Task_handler.
	 *
	 * Parses the config file using the parser and constructs specialized Communicator and Hypervisor.
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
private:
	std::shared_ptr<Communicator> comm;
	std::shared_ptr<Hypervisor> hypervisor;
	bool running;
};

#endif
