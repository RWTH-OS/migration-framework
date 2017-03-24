/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef DUMMY_HYPERVISOR_HPP
#define DUMMY_HYPERVISOR_HPP

#include "hypervisor.hpp"

/**
 * \brief Implementation of the Hypervisor interface not doing anything.
 *
 * This class implements the Hypervisor interface.
 * It provides methods to start, stop and migrate virtual machines.
 * It does not really do anything, but therefore never throws if never_throw is set else always throws.
 * Only for test purposes.
 */
class Dummy_hypervisor :
	public Hypervisor
{
public:
	/**
	 * \brief Constructor of Dummy_hypervisor.
	 *
	 * \param always_success Specify wheather start/stop/migrate methods succeed (true) or throw (false)
	 */
	Dummy_hypervisor(bool never_throw) noexcept;
	/**
	 * \brief Method to start a virtual machine.
	 *
	 * Dummy method that does not do anything.
	 * Never throws if never_throw is true, else it throws.
	 * \param vm_name The name of the vm to start.
	 * \param vcpus The number of virtual cpus to be assigned to the vm.
	 * \param memory The amount of ram memory to be assigned to the vm in KiB.
	 */
	void start(const fast::msg::migfra::Start &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to stop a virtual machine.
	 *
	 * Dummy method that does not do anything.
	 * Never throws if never_throw is true, else it throws.
	 * \param vm_name The name of the vm to stop.
	 */
	void stop(const fast::msg::migfra::Stop &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to migrate a virtual machine to another host.
	 *
	 * Dummy method that does not do anything.
	 * Never throws if never_throw is true, else it throws.
	 * \param vm_name The name of the vm to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Enables live migration.
	 * \param rdma_migration Enables rdma migration.
	 */
	void migrate(const fast::msg::migfra::Migrate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm) override;
	/**
	 * \brief Method to repin vcpus of a virtual machine.
	 *
	 * Dummy method that does not do anything.
	 * Never throws if never_throw is true, else it throws.
	 */
	void repin(const fast::msg::migfra::Repin &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to suspend the execution of a virtual machine.
	 *
	 * Dummy method that does not do anything.
	 * Never throws if never_throw is true, else it throws.
	 */
	void suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to resume the execution of a virtual machine.
	 *
	 * Dummy mresume that does not do anything.
	 * Never throws if never_throw is true, else it throws.
	 */
	void resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement) override;

private:
	const bool never_throw;
};

#endif
