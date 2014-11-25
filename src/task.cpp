#include "task.hpp"

#include "logging.hpp"

Start::Start(const std::string &vm_name, size_t vcpus, size_t memory) :
	vm_name(vm_name),
	vcpus(vcpus),
	memory(memory)
{
}

/// TODO: Call start function of hypervisor.
void Start::operator()()
{
	LOG_PRINT(LOG_NOTICE, "Executing start task.");
	LOG_PRINT(LOG_ERR, "No implementation of start task!");
}

Start_packed::Start_packed(const std::forward_list<Start> &start_tasks) :
	start_tasks(start_tasks)
{
}

void Start_packed::operator()()
{
	for (auto &start_vm : start_tasks) {
		start_vm();
	}
}

Stop::Stop(const std::string &vm_name) :
	vm_name(vm_name)
{
}

/// TODO: Call stop function of hypervisor.
void Stop::operator()()
{
	LOG_PRINT(LOG_NOTICE, "Executing stop task.");
	LOG_PRINT(LOG_ERR, "No implementation of stop task!");
}

Migrate::Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration) :
	vm_name(vm_name),
	dest_hostname(dest_hostname),
	live_migration(live_migration)
{
}

/// TODO: Call migrate function of hypervisor.
void Migrate::operator()()
{
	LOG_PRINT(LOG_NOTICE, "Executing migration task.");
	LOG_PRINT(LOG_ERR, "No implementation of migrate task!");
}
