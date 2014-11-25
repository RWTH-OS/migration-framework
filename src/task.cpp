#include "task.hpp"

#include "logging.hpp"

Result::Result(const std::string &title, const std::string &vm_name, const std::string &status) :
	title(title),
	vm_name(vm_name),
	status(status)
{
}

Start::Start(const std::string &vm_name, size_t vcpus, size_t memory) :
	vm_name(vm_name),
	vcpus(vcpus),
	memory(memory)
{
}

/// TODO: Call start function of hypervisor.
std::vector<Result> Start::execute()
{
	LOG_PRINT(LOG_NOTICE, "Executing start task.");
	LOG_PRINT(LOG_ERR, "No implementation of start task!");
	return {Result("vm started", vm_name, "error")};
}

Start_packed::Start_packed(const std::vector<Start> &start_tasks) :
	start_tasks(start_tasks)
{
}

std::vector<Result> Start_packed::execute()
{
	std::vector<Result> results;
	for (auto &start_vm : start_tasks) {
		results.push_back(start_vm.execute().front());
	}
	return results;
}

Stop::Stop(const std::string &vm_name) :
	vm_name(vm_name)
{
}

/// TODO: Call stop function of hypervisor.
std::vector<Result> Stop::execute()
{
	LOG_PRINT(LOG_NOTICE, "Executing stop task.");
	LOG_PRINT(LOG_ERR, "No implementation of stop task!");
	return {Result("vm stopped", vm_name, "error")};
}

Stop_packed::Stop_packed(const std::vector<Stop> &stop_tasks) :
	stop_tasks(stop_tasks)
{
}

/// TODO: Call stop function of hypervisor.
std::vector<Result> Stop_packed::execute()
{
	std::vector<Result> results;
	for (auto &stop_vm : stop_tasks) {
		results.push_back(stop_vm.execute().front());
	}
	return results;
}

Migrate::Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration) :
	vm_name(vm_name),
	dest_hostname(dest_hostname),
	live_migration(live_migration)
{
}

/// TODO: Call migrate function of hypervisor.
std::vector<Result> Migrate::execute()
{
	LOG_PRINT(LOG_NOTICE, "Executing migration task.");
	LOG_PRINT(LOG_ERR, "No implementation of migrate task!");
	return {Result("migrate done", vm_name, "error")};
}
