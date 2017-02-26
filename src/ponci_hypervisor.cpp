/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "ponci_hypervisor.hpp"

#include <ponci/ponci.hpp>

void Ponci_hypervisor::start(const fast::msg::migfra::Start &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;

	// Create cgroup
	std::string cgroup_name = task.vm_name.get();
	try {
		cgroup_create(cgroup_name);
	} catch (const std::exception &e) {
	        throw std::runtime_error(
	            "Exception while creating cgroup: " +
	            std::string(e.what()));
	}
}

void Ponci_hypervisor::stop(const fast::msg::migfra::Stop &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;

	// Delete cgroup
	std::string cgroup_name = task.vm_name.get();
	try {
		cgroup_delete(cgroup_name);
	} catch (const std::exception &e) {
	        throw std::runtime_error(
	            "Exception while deleting cgroup: " +
	            std::string(e.what()));
	}

}

void Ponci_hypervisor::migrate(const fast::msg::migfra::Migrate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm)
{
	(void) task; (void) time_measurement; (void) comm;
	throw std::runtime_error("Ponci_hypervisor has no supoprt for migrations.");
}

void Ponci_hypervisor::repin(const fast::msg::migfra::Repin &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto cgroup_name = task.vm_name;
	auto &cpu_map = task.vcpu_map;

	// Check if more than one map is provided
	if (cpu_map.size() != 1)
		throw std::runtime_error("Ponci_hypervisor only supports one dimensional cpu maps.");

	// Add cpus to cgroup
	std::vector<size_t> cpus(cpu_map[0].begin(), cpu_map[0].end());
	try {
		cgroup_set_cpus(cgroup_name, cpus);
	} catch (const std::exception &e) {
	        throw std::runtime_error(
	            "Exception while setting cpus: " +
	            std::string(e.what()));
	}
}

void Ponci_hypervisor::suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto cgroup_name = task.vm_name;

	// Freeze cgroup
	try {
		cgroup_freeze(cgroup_name);
		cgroup_wait_frozen(cgroup_name);
	} catch (const std::exception &e) {
	        throw std::runtime_error(
	            "Exception while freezing cgroup: " +
	            std::string(e.what()));
	}
}

void Ponci_hypervisor::resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto cgroup_name = task.vm_name;

	// Thaw cgroup
	try {
		cgroup_thaw(cgroup_name);
		cgroup_wait_thawed(cgroup_name);
	} catch (const std::exception &e) {
	        throw std::runtime_error(
	            "Exception while thawing cgroup: " +
	            std::string(e.what()));
	}
}
