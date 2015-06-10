/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "task.hpp"

#include "hooks.hpp"

#include <exception>
#include <future>
#include <utility>
#include <iostream>
#include <array>

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

Result_container::Result_container(const std::string &yaml_str)
{
	from_string(yaml_str);
}

Result_container::Result_container(const std::string &title, const std::vector<Result> &results) :
	title(title),
	results(results)
{
}

YAML::Node Result_container::emit() const
{
	YAML::Node node;
	node["result"] = title;
	node["list"] = results;
	return node;
}

void Result_container::load(const YAML::Node &node)
{
	fast::load(title, node["result"]);
	fast::load(results, node["list"]);
}

Result::Result(const std::string &vm_name, const std::string &status, const std::string &details) :
	vm_name(vm_name),
	status(status),
	details(details)
{
}

YAML::Node Result::emit() const
{
	YAML::Node node;
	node["vm-name"] = vm_name;
	node["status"] = status;
	if (details != "")
		node["details"] = details;
	return node;
}

void Result::load(const YAML::Node &node)
{
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

std::string Task::type(bool enable_result_format) const
{
	std::array<std::string, 3> types;
	if (enable_result_format)
		types = {"vm started", "vm stopped", "vm migrated"};
	else
		types = {"start vm", "stop vm", "migrate vm"};
	if (sub_tasks.empty())
		throw std::runtime_error("No subtasks available to get type.");
	else if (std::dynamic_pointer_cast<Start>(sub_tasks.front()))
		return types[0];
	else if (std::dynamic_pointer_cast<Stop>(sub_tasks.front()))
		return types[1];
	else if (std::dynamic_pointer_cast<Migrate>(sub_tasks.front()))
		return types[2];
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

void Task::execute(const std::shared_ptr<Hypervisor> &hypervisor, const std::shared_ptr<fast::Communicator> &comm)
{
	if (sub_tasks.empty()) return;
	/// \todo In C++14 unique_ptr for sub_tasks and init capture to move in lambda should be used!
	auto &sub_tasks = this->sub_tasks;
	auto result_type = type();
	auto func = [&hypervisor, &comm, sub_tasks, result_type] 
	{
		std::vector<std::future<Result>> future_results;
		for (auto &sub_task : sub_tasks) // start subtasks
			future_results.push_back(sub_task->execute(hypervisor));
		std::vector<Result> results;
		for (auto &future_result : future_results) // wait for subtasks to finish
			results.push_back(future_result.get());
		comm->send_message(Result_container(result_type, results).to_string());
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
	node["name"] = vm_name;
	node["vcpus"] = vcpus;
	node["memory"] = memory;
	return node;
}

void Start::load(const YAML::Node &node)
{
	Sub_task::load(node);
	fast::load(vm_name, node["name"]);
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
			return Result(vm_name, "error", e.what());
		}
		return Result(vm_name, "success");
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
			return Result(vm_name, "error", e.what());
		}
		return Result(vm_name, "success");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}

Migrate::Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool concurrent_execution, unsigned int pscom_hook_procs) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name),
	dest_hostname(dest_hostname),
	live_migration(live_migration),
	pscom_hook_procs(pscom_hook_procs)
{
}

YAML::Node Migrate::emit() const
{
	YAML::Node node = Sub_task::emit();
	node["vm-name"] = vm_name;
	node["destination"] = dest_hostname;
	node["parameter"]["live-migration"] = live_migration;
	node["parameter"]["pscom-hook-procs"] = pscom_hook_procs;
	return node;
}

void Migrate::load(const YAML::Node &node)
{
	Sub_task::load(node);
	fast::load(vm_name, node["vm-name"]);
	fast::load(dest_hostname, node["destination"]);
	fast::load(live_migration, node["parameter"]["live-migration"]);
	fast::load(pscom_hook_procs, node["parameter"]["pscom-hook-procs"]);
}

std::future<Result> Migrate::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto &dest_hostname = this->dest_hostname;
	auto &live_migration = this->live_migration;
	auto &pscom_hook_procs = this->pscom_hook_procs;
	auto func = [&hypervisor, vm_name, dest_hostname, live_migration, pscom_hook_procs] ()
	{
		try {
			// Suspend pscom
			Suspend_pscom pscom_hook(vm_name, pscom_hook_procs);
			// Start migration
			hypervisor->migrate(vm_name, dest_hostname, live_migration);
			// Resume pscom
			pscom_hook.resume();
		} catch (const std::exception &e) {
			return Result(vm_name, "error", e.what());
		}
		return Result(vm_name, "success");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}
