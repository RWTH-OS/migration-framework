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
#include <regex>

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
	FASTLIB_LOG(migfra_task_log, trace) << "Waiting for threads to finish...";
	std::unique_lock<std::mutex> lock(count_mutex);
	while (count != 0)
		count_cv.wait(lock);
	FASTLIB_LOG(migfra_task_log, trace) << "All threads are finished.";
}

unsigned int Thread_counter::count;
std::mutex Thread_counter::count_mutex;
std::condition_variable Thread_counter::count_cv;

void send_parse_error(std::shared_ptr<fast::Communicator> comm, const std::string &msg, const std::string &id)
{
	FASTLIB_LOG(migfra_task_log, warn) << msg;
	comm->send_message(Result_container("unknown", {Result("unknown", "error", msg)}, id).to_string());
}

void send_parse_error_nothrow(std::shared_ptr<fast::Communicator> comm, const std::string &msg, const std::string &id)
{
	try {
		send_parse_error(comm, msg, id);
	} catch (...) {
		FASTLIB_LOG(migfra_task_log, trace) << "Exception while sending error message.";
	}
}

void send_quit_result(std::shared_ptr<fast::Communicator> comm, const std::string &id)
{
	comm->send_message(Result_container("quit", {Result("n/a", "success")}, id).to_string());
}

std::future<Result> execute(std::shared_ptr<Task> task,
		std::shared_ptr<Hypervisor> hypervisor,
		std::shared_ptr<fast::Communicator> comm)
{
	auto func = [task, hypervisor, comm]
	{
		Time_measurement time_measurement(task->time_measurement.is_valid() ? task->time_measurement.get() : false);
		std::string vm_name;
		auto start_task = std::dynamic_pointer_cast<Start>(task);
		auto stop_task = std::dynamic_pointer_cast<Stop>(task);
		auto migrate_task = std::dynamic_pointer_cast<Migrate>(task);
		auto repin_task = std::dynamic_pointer_cast<Repin>(task);
		auto suspend_task = std::dynamic_pointer_cast<Suspend>(task);
		auto resume_task = std::dynamic_pointer_cast<Resume>(task);
		try {
			time_measurement.tick("overall");
			if (start_task) {
				if (start_task->vm_name.is_valid())
					vm_name = start_task->vm_name.get();
				else if (start_task->xml.is_valid()) {
					std::regex regex(R"(<name>(.+)</name>)");
					auto xml = start_task->xml.get();
					std::smatch match;
					auto found = std::regex_search(xml, match, regex);
					if (found && match.size() == 2) {
						vm_name = match[1].str();
						start_task->vm_name = vm_name;
					} else {
						vm_name = xml;
						throw std::runtime_error("Could not find vm-name in xml.");
					}
				} else if (start_task->base_name.is_valid()) {
					vm_name = start_task->base_name.get();
				}
				hypervisor->start(*start_task, time_measurement);
			} else if (stop_task) {
				if (stop_task->vm_name)
					vm_name = *stop_task->vm_name;
				else if (stop_task->regex)
					vm_name = *stop_task->regex;
				else
					throw std::runtime_error("Neither vm-name or regex is defined in stop task.");
				hypervisor->stop(*stop_task, time_measurement);
			} else if (migrate_task) {
				vm_name = migrate_task->vm_name;
				hypervisor->migrate(*migrate_task, time_measurement, comm);
			} else if (repin_task) {
				vm_name = repin_task->vm_name;
				hypervisor->repin(*repin_task, time_measurement);
			} else if (suspend_task) {
				vm_name = suspend_task->vm_name;
				hypervisor->suspend(*suspend_task, time_measurement);
			} else if (resume_task) {
				vm_name = resume_task->vm_name;
				hypervisor->resume(*resume_task, time_measurement);
			}
		} catch (const std::exception &e) {
			FASTLIB_LOG(migfra_task_log, warn) << "Exception in task: " << e.what();
			return Result(vm_name, "error", time_measurement, e.what());
		}
		time_measurement.tock("overall");
		return Result(vm_name, "success", time_measurement);
	};
	bool concurrent_execution = !task->concurrent_execution.is_valid() || task->concurrent_execution.get();
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}


void execute(const Task_container &task_cont, std::shared_ptr<Hypervisor> hypervisor, std::shared_ptr<fast::Communicator> comm)
{
	auto &id = task_cont.id.is_valid() ? task_cont.id.get() : "";
	if (task_cont.tasks.empty()) {
		send_parse_error(comm, "Empty task container executed.", id);
		return;
	}
	/// \todo In C++14 unique_ptr for sub_tasks and init capture to move in lambda should be used!
	auto &tasks = task_cont.tasks;
	auto result_type = task_cont.type(true);
	if (result_type == "quit") {
		send_quit_result(comm, id);
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
		comm->send_message(Result_container(result_type, results, id).to_string());
	};
	bool concurrent_execution = !task_cont.concurrent_execution.is_valid() || task_cont.concurrent_execution.get();
	concurrent_execution ? std::thread([func] {Thread_counter cnt; func();}).detach() : func();
}

