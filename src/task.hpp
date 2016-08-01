/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef TASK_HPP
#define TASK_HPP

#include "hypervisor.hpp"

#include <fast-lib/communicator.hpp>
#include <fast-lib/message/migfra/task.hpp>

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>

/**
 * \brief RAII-style thread counter.
 *
 * Each instance of Thread_counter represents a running thread.
 * The constructor increments and the destructor decrements the counter.
 * A condition variable is used to wait for the counter to become zero.
 * 
 * TODO: Move static member variables to seperate class to provide multiple thread counters.
 * TODO: Proper testing.
 */
class Thread_counter
{
public:
	/**
	 * \brief Constructor increments counter.
	 */
	Thread_counter();

	/**
	 * \brief Destructor decrements counter.
	 */
	~Thread_counter();

	/**
	 * \brief Wait until count is zero using a condition variable.
	 */
	static void wait_for_threads_to_finish();
private:
	static unsigned int count;
	static std::mutex count_mutex;
	static std::condition_variable count_cv;
};

void send_parse_error(std::shared_ptr<fast::Communicator> comm, const std::string &msg, const std::string &id = "");

void send_parse_error_nothrow(std::shared_ptr<fast::Communicator> comm, const std::string &msg, const std::string &id = "");

void execute(const fast::msg::migfra::Task_container &task_cont, 
		std::shared_ptr<Hypervisor> hypervisor, 
		std::shared_ptr<fast::Communicator> comm);

#endif
