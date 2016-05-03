/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task.hpp"

#include "pscom_handler.hpp"

#include <fast-lib/message/migfra/result.hpp>
#include <fast-lib/log.hpp>

#include <exception>
#include <future>
#include <utility>
#include <iostream>
#include <array>

FASTLIB_LOG_INIT(migfra_task_log, "Task")
FASTLIB_LOG_SET_LEVEL_GLOBAL(migfra_task_log, trace);

using namespace fast::msg::migfra;

Thread_counter::Thread_counter()
{
	std::unique_lock<std::mutex> lock(count_mutex);
	++count;
}

Thread_counter::~Thread_counter()
{
	std::unique_lock<std::mutex> lock(count_mutex);
	if (--count == 0)
		count_cv.notify_one();
}

void Thread_counter::wait_for_threads_to_finish()
{
	std::cout << "Debug: Waiting for threads to finish..." << std::endl;
	std::unique_lock<std::mutex> lock(count_mutex);
	while (count != 0)
		count_cv.wait(lock);
	std::cout << "Debug: All threads are finished." << std::endl;
}

unsigned int Thread_counter::count;
std::mutex Thread_counter::count_mutex;
std::condition_variable Thread_counter::count_cv;

std::future<Result> execute(std::shared_ptr<Task> task, 
		std::shared_ptr<Hypervisor> hypervisor, 
		std::shared_ptr<fast::Communicator> comm)
{
	auto func = [task, hypervisor, comm]
	{
		fast::msg::migfra::Time_measurement time_measurement(task->time_measurement.is_valid() ? task->time_measurement.get() : false);
		try {
			time_measurement.tick("overall");
			auto start_task = std::dynamic_pointer_cast<Start>(task);
			auto stop_task = std::dynamic_pointer_cast<Stop>(task);
			auto migrate_task = std::dynamic_pointer_cast<Migrate>(task);
			if (start_task) {
				hypervisor->start(*start_task, time_measurement);
			} else if (stop_task) {
				hypervisor->stop(*stop_task, time_measurement);
			} else if (migrate_task) {
				// Suspend pscom (resume in destructor)
				// TODO: pass whole migrate task
				Pscom_handler pscom_handler(*migrate_task, comm, time_measurement);
				// Start migration
				hypervisor->migrate(*migrate_task, time_measurement);
			}
		} catch (const std::exception &e) {
			FASTLIB_LOG(migfra_task_log, warning) << "Exception in task: " << e.what();
			return Result(task->vm_name, "error", time_measurement, e.what());
		}
		time_measurement.tock("overall");
		return Result(task->vm_name, "success", time_measurement);
	};
	bool concurrent_execution = !task->concurrent_execution.is_valid() || task->concurrent_execution.get();
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}


void execute(const Task_container &task_cont, std::shared_ptr<Hypervisor> hypervisor, std::shared_ptr<fast::Communicator> comm)
{
	if (task_cont.tasks.empty()) {
		FASTLIB_LOG(migfra_task_log, warning) << "Empty task container executed.";
		return;
	}
	/// \todo In C++14 unique_ptr for sub_tasks and init capture to move in lambda should be used!
	auto &tasks = task_cont.tasks;
	auto result_type = task_cont.type(true);
	auto &id = task_cont.id;
	if (result_type == "quit") {
		throw std::runtime_error("quit");
	}
	auto func = [hypervisor, comm, tasks, result_type, id]
	{
		std::vector<std::future<Result>> future_results;
		for (auto &task : tasks) // start subtasks
			future_results.push_back(execute(task, hypervisor, comm));
		std::vector<Result> results;
		for (auto &future_result : future_results) // wait for tasks to finish
			results.push_back(future_result.get());
		if (id.is_valid())
			comm->send_message(Result_container(result_type, results, id).to_string());
		else
			comm->send_message(Result_container(result_type, results).to_string());
	};
	bool concurrent_execution = !task_cont.concurrent_execution.is_valid() || task_cont.concurrent_execution.get();
	concurrent_execution ? std::thread([func] {Thread_counter cnt; func();}).detach() : func();
}

