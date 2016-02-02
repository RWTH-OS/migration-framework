/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task.hpp"

#include "hooks.hpp"

#include <fast-lib/message/migfra/result.hpp>

#include <boost/log/trivial.hpp>

#include <exception>
#include <future>
#include <utility>
#include <iostream>
#include <array>

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
		fast::msg::migfra::Time_measurement time_measurement(task->time_measurement);
		try {
			time_measurement.tick("overall");
			auto start_task = std::dynamic_pointer_cast<Start>(task);
			auto stop_task = std::dynamic_pointer_cast<Stop>(task);
			auto migrate_task = std::dynamic_pointer_cast<Migrate>(task);
			if (start_task) {
				hypervisor->start(start_task->vm_name, 
						start_task->vcpus, 
						start_task->memory, 
						start_task->pci_ids);
			} else if (stop_task) {
				hypervisor->stop(stop_task->vm_name, 
						stop_task->force);
			} else if (migrate_task) {
				// Suspend pscom (resume in destructor)
				Suspend_pscom pscom_hook(migrate_task->vm_name,
						migrate_task->pscom_hook_procs, comm,
						time_measurement);
				// Start migration
				hypervisor->migrate(migrate_task->vm_name, 
						migrate_task->dest_hostname,
						migrate_task->live_migration, 
						migrate_task->rdma_migration, 
						time_measurement);
			}
		} catch (const std::exception &e) {
			BOOST_LOG_TRIVIAL(warning) << "Exception in task: " << e.what();
			return Result(task->vm_name, "error", time_measurement, e.what());
		}
		time_measurement.tock("overall");
		return Result(task->vm_name, "success", time_measurement);
	};
	return std::async(task->concurrent_execution ? std::launch::async : std::launch::deferred, func);
}


void execute(const Task_container &task_cont, std::shared_ptr<Hypervisor> hypervisor, std::shared_ptr<fast::Communicator> comm)
{
	if (task_cont.tasks.empty()) {
		BOOST_LOG_TRIVIAL(warning) << "Empty task container executed.";
		return;
	}
	/// \todo In C++14 unique_ptr for sub_tasks and init capture to move in lambda should be used!
	auto &tasks = task_cont.tasks;
	auto result_type = task_cont.type(true);
	if (result_type == "quit") {
		throw std::runtime_error("quit");
	}
	auto func = [hypervisor, comm, tasks, result_type]
	{
		std::vector<std::future<Result>> future_results;
		for (auto &task : tasks) // start subtasks
			future_results.push_back(execute(task, hypervisor, comm));
		std::vector<Result> results;
		for (auto &future_result : future_results) // wait for tasks to finish
			results.push_back(future_result.get());
		comm->send_message(Result_container(result_type, results).to_string());
	};
	task_cont.concurrent_execution ? std::thread([func] {Thread_counter cnt; func();}).detach() : func();
}

