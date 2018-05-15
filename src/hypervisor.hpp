/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef HYPERVISOR_HPP
#define HYPERVISOR_HPP

#include <fast-lib/message/migfra/task.hpp>
#include <fast-lib/message/migfra/pci_id.hpp>
#include <fast-lib/message/migfra/time_measurement.hpp>
#include <fast-lib/communicator.hpp>
using PCI_id = fast::msg::migfra::PCI_id;
using Time_measurement = fast::msg::migfra::Time_measurement;

#include <string>
#include <vector>

/**
 * \brief An abstract class to provide an interface for the hypervisor.
 *
 * This interface provides methods to start, stop and migrate virtual machines.
 */
class Hypervisor
{
public:
	/**
	 * \brief Default virtual destructor.
	 */
	virtual ~Hypervisor() = default;
	/**
	 * \brief Method to start a virtual machine.
	 *
	 * A pure virtual method to provide an interface for starting a virtual machine.
	 * \param vm_name The name of the vm to start.
	 * \param vcpus The number of virtual cpus to be assigned to the vm.
	 * \param memory The amount of ram memory to be assigned to the vm in KiB.
	 */
	virtual void start(const fast::msg::migfra::Start &task, fast::msg::migfra::Time_measurement &time_measurement) = 0;
	/**
	 * \brief Method to stop a virtual machine.
	 *
	 * A pure virtual method to provide an interface for stopping a virtual machine.
	 * \param vm_name The name of the vm to stop.
	 */
	virtual void stop(const fast::msg::migfra::Stop &task, fast::msg::migfra::Time_measurement &time_measurement) = 0;
	/**
	 * \brief Method to migrate a virtual machine to another host.
	 *
	 * A pure virtual method to provide an interface for migrating a virtual machine to another host.
	 * \param vm_name The name of the vm to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Enables live migration.
	 * \param rdma_migration Enables rdma migration.
	 */
	virtual void migrate(const fast::msg::migfra::Migrate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm) = 0;
	/**
	 * \brief Method to evacuate a host.
	 */
	virtual void evacuate(const fast::msg::migfra::Evacuate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm) = 0;
	/**
	 * \brief Method to repin vcpus of a virtual machine.
	 *
	 * Calls libvirt API to reassign CPUs to VCPUs.
	 */
	virtual void repin(const fast::msg::migfra::Repin &task, fast::msg::migfra::Time_measurement &time_measurement) = 0;
	/**
	 * \brief Method to suspend the execution of a virtual machine.
	 *
	 * A pure virtual method to provide an interface for suspending the execution of a virtual machine.
	 */
	virtual void suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement) = 0;
	/**
	 * \brief Method to resume the execution of a virtual machine.
	 *
	 * A pure virtual method to provide an interface for resuming the execution of a virtual machine.
	 */
	virtual void resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement) = 0;
	/**
 	 * \brief Method to generate a task list for Evacuate.
 	 */
	virtual std::vector<std::shared_ptr<fast::msg::migfra::Task>> get_evacuate_tasks(const fast::msg::migfra::Task_container &task_cont) = 0;
};

#endif
