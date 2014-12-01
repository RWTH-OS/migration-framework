#include "task.hpp"

#include "logging.hpp"

#include <exception>

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

std::vector<Result> Start::execute(const std::unique_ptr<Hypervisor> &hypervisor)
{
	LOG_PRINT(LOG_DEBUG, "Executing start task.");
	try {
		hypervisor->start(vm_name, vcpus, memory);
	} catch (const std::exception &e) {
		return {Result("vm started", vm_name, "error")};
	}
	return {Result("vm started", vm_name, "success")};
}

Start_packed::Start_packed(const std::vector<Start> &start_tasks) :
	start_tasks(start_tasks)
{
}

std::vector<Result> Start_packed::execute(const std::unique_ptr<Hypervisor> &hypervisor)
{
	std::vector<Result> results;
	for (auto &start_vm : start_tasks) {
		results.push_back(start_vm.execute(hypervisor).front());
	}
	return results;
}

Stop::Stop(const std::string &vm_name) :
	vm_name(vm_name)
{
}

std::vector<Result> Stop::execute(const std::unique_ptr<Hypervisor> &hypervisor)
{
	LOG_PRINT(LOG_DEBUG, "Executing stop task.");
	try {
		hypervisor->stop(vm_name);
	} catch (const std::exception &e) {
		return {Result("vm stopped", vm_name, "error")};
	}
	return {Result("vm stopped", vm_name, "success")};
}

Stop_packed::Stop_packed(const std::vector<Stop> &stop_tasks) :
	stop_tasks(stop_tasks)
{
}

std::vector<Result> Stop_packed::execute(const std::unique_ptr<Hypervisor> &hypervisor)
{
	std::vector<Result> results;
	for (auto &stop_vm : stop_tasks) {
		results.push_back(stop_vm.execute(hypervisor).front());
	}
	return results;
}

Migrate::Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration) :
	vm_name(vm_name),
	dest_hostname(dest_hostname),
	live_migration(live_migration)
{
}

std::vector<Result> Migrate::execute(const std::unique_ptr<Hypervisor> &hypervisor)
{
	LOG_PRINT(LOG_DEBUG, "Executing migration task.");
	try {
		hypervisor->migrate(vm_name, dest_hostname, live_migration);
	} catch (const std::exception &e) {
		return {Result("migrate done", vm_name, "error")};
	}
	return {Result("migrate done", vm_name, "success")};
}
