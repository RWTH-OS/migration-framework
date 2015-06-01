/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task.hpp"

#include "parser.hpp"
#include "logging.hpp"

#include <exception>
#include <future>
#include <utility>
#include <iostream>

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
	LOG_PRINT(LOG_DEBUG, "Waiting for threads to finish...");
	std::unique_lock<std::mutex> lock(count_mutex);
	while (count != 0)
		count_cv.wait(lock);
	LOG_PRINT(LOG_DEBUG, "All threads are finished.");
}

unsigned int Thread_counter::count;
std::mutex Thread_counter::count_mutex;
std::condition_variable Thread_counter::count_cv;


Result::Result(const std::string &title, const std::string &vm_name, const std::string &status, const std::string &details) :
	title(title),
	vm_name(vm_name),
	status(status),
	details(details)
{
}

YAML::Node Result::emit() const
{
	YAML::Node node;
	node["result"] = title;
	node["vm-name"] = vm_name;
	node["status"] = status;
	if (details != "")
		node["details"] = details;
	return node;
}

void Result::load(const YAML::Node &node)
{
	fast::load(title, node["result"]);
	fast::load(vm_name, node["vm-name"]);
	fast::load(status, node["status"]);
	fast::load(details, node["details"], "");
}

Sub_task::Sub_task(bool concurrent_execution) :
	concurrent_execution(concurrent_execution)
{
}

YAML::Node Sub_task::emit() const
{
	YAML::Node node;
	node["concurrent-execution"] = concurrent_execution;
	return node;
}

void Sub_task::load(const YAML::Node &node)
{
	fast::load(concurrent_execution, node["concurrent-execution"], true);
}

Task::Task(std::vector<std::shared_ptr<Sub_task>> sub_tasks, bool concurrent_execution) :
	sub_tasks(std::move(sub_tasks)), concurrent_execution(concurrent_execution)
{
}

std::string Task::type() const
{
	if (sub_tasks.empty())
		throw std::runtime_error("No subtasks available to get test type.");
	else if (std::dynamic_pointer_cast<Start>(sub_tasks.front()))
		return "start vm";
	else if (std::dynamic_pointer_cast<Stop>(sub_tasks.front()))
		return "stop vm";
	else if (std::dynamic_pointer_cast<Migrate>(sub_tasks.front()))
		return "migrate vm";
	else
		throw std::runtime_error("Unknown type of Task.");

}

YAML::Node Task::emit() const
{
	YAML::Node node;
	node["task"] = type();
	node["vm-configurations"] = sub_tasks;
	node["concurrent-execution"] = concurrent_execution;
	return node;
}

template<class T> std::vector<std::shared_ptr<Sub_task>> load_sub_tasks(const YAML::Node &node)
{
	std::vector<std::shared_ptr<T>> sub_tasks;
	fast::load(sub_tasks, node["vm-configurations"]);
	return std::vector<std::shared_ptr<Sub_task>>(sub_tasks.begin(), sub_tasks.end());
}

// Specialization for Migrate due to different yaml structure (no "vm-configurations")
template<> std::vector<std::shared_ptr<Sub_task>> load_sub_tasks<Migrate>(const YAML::Node &node)
{
	std::shared_ptr<Migrate> migrate_task;
	fast::load(migrate_task, node);
	return std::vector<std::shared_ptr<Sub_task>>(1, migrate_task);
}

void Task::load(const YAML::Node &node)
{
	std::string type;
	try {
		fast::load(type, node["task"]);
	} catch (const std::exception &e) {
		throw Task::no_task_exception("Cannot find key \"task\" to load Task from YAML.");
	}
	if (type == "start vm") {
		sub_tasks = load_sub_tasks<Start>(node);
	}
	else if (type == "stop vm") {
		sub_tasks = load_sub_tasks<Stop>(node);
	}
	else if (type == "migrate vm") {
		sub_tasks = load_sub_tasks<Migrate>(node);
	}
	else
		throw std::runtime_error("Unknown type of Task while loading.");
	fast::load(concurrent_execution, node["concurrent-execution"], true);
}

void Task::execute(const std::shared_ptr<Hypervisor> &hypervisor, const std::shared_ptr<Communicator> &comm)
{
	if (sub_tasks.empty()) return;
	/// \todo In C++14 unique_ptr for sub_tasks and init capture to move in lambda should be used!
	auto &sub_tasks = this->sub_tasks;
	auto func = [&hypervisor, &comm, sub_tasks] 
	{
		std::vector<std::future<Result>> future_results;
		for (auto &sub_task : sub_tasks) // start subtasks
			future_results.push_back(sub_task->execute(hypervisor));
		std::vector<Result> results;
		for (auto &future_result : future_results) // wait for subtasks to finish
			results.push_back(future_result.get());
		comm->send_message(parser::results_to_str(results));
	};
	concurrent_execution ? std::thread([func]{Thread_counter cnt; func();}).detach() : func();
}

Start::Start(const std::string &vm_name, unsigned int vcpus, unsigned long memory, bool concurrent_execution) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name),
	vcpus(vcpus),
	memory(memory)
{
}

YAML::Node Start::emit() const
{
	YAML::Node node = Sub_task::emit();
	node["vm-name"] = vm_name;
	node["vcpus"] = vcpus;
	node["memory"] = memory;
	return node;
}

void Start::load(const YAML::Node &node)
{
	Sub_task::load(node);
	fast::load(vm_name, node["vm-name"]);
	fast::load(vcpus, node["vcpus"]);
	fast::load(memory, node["memory"]);
}

std::future<Result> Start::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto &vcpus = this->vcpus;
	auto &memory = this->memory;
	auto func = [&hypervisor, vm_name, vcpus, memory] () 
	{
		try {
			hypervisor->start(vm_name, vcpus, memory);
		} catch (const std::exception &e) {
			return Result("vm started", vm_name, "error", e.what());
		}
		return Result("vm started", vm_name, "success", "");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}

Stop::Stop(const std::string &vm_name, bool concurrent_execution) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name)
{
}

YAML::Node Stop::emit() const
{
	YAML::Node node = Sub_task::emit();
	node["vm-name"] = vm_name;
	return node;
}

void Stop::load(const YAML::Node &node)
{
	Sub_task::load(node);
	fast::load(vm_name, node["vm-name"]);
}

std::future<Result> Stop::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto func = [&hypervisor, vm_name] ()
	{
		try {
			hypervisor->stop(vm_name);
		} catch (const std::exception &e) {
			return Result("vm stopped", vm_name, "error", e.what());
		}
		return Result("vm stopped", vm_name, "success", "");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}

Migrate::Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool concurrent_execution) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name),
	dest_hostname(dest_hostname),
	live_migration(live_migration)
{
}

YAML::Node Migrate::emit() const
{
	YAML::Node node = Sub_task::emit();
	node["vm-name"] = vm_name;
	node["destination"] = dest_hostname;
	node["live-migration"] = live_migration;
	return node;
}

void Migrate::load(const YAML::Node &node)
{
	Sub_task::load(node);
	fast::load(vm_name, node["vm-name"]);
	fast::load(dest_hostname, node["destination"]);
	fast::load(live_migration, node["live-migration"]);
}

std::future<Result> Migrate::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto &dest_hostname = this->dest_hostname;
	auto &live_migration = this->live_migration;
	auto func = [&hypervisor, vm_name, dest_hostname, live_migration] ()
	{
		try {
			hypervisor->migrate(vm_name, dest_hostname, live_migration);
		} catch (const std::exception &e) {
			return Result("migrate done", vm_name, "error", e.what());
		}
		return Result("migrate done", vm_name, "success", "");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}
