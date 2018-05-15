/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef PONCI_HYPERVISOR_HPP
#define PONCI_HYPERVISOR_HPP

#include "hypervisor.hpp"

/**
 * \brief Implementation of the Hypervisor interface not doing anything.
 *
 * This class implements the Hypervisor interface.
 * It provides methods to start, stop and migrate virtual machines.
 */
class Ponci_hypervisor :
	public Hypervisor
{
public:
	/**
	 * \brief Constructor of Ponci_hypervisor.
	 */
	Ponci_hypervisor(void) = default;
	/**
	 * \brief Method to create a cgroup.
	 */
	void start(const fast::msg::migfra::Start &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to delete a croup.
	 */
	void stop(const fast::msg::migfra::Stop &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method not supported.
	 */
	void migrate(const fast::msg::migfra::Migrate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm) override;
	/**
 	 * \brief Method to evacuate a host.
 	 */
	void evacuate(const fast::msg::migfra::Evacuate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm) override;
	/**
	 * \brief Method to set cpus of a cgroup.
	 */
	void repin(const fast::msg::migfra::Repin &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to freeze a cgroup.
	 */
	void suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
	 * \brief Method to thaw a cgroup.
	 */
	void resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement) override;
	/**
 	 * \brief Method to generate a task list for Evacuate.
 	 */
	std::vector<std::shared_ptr<fast::msg::migfra::Task>> get_evacuate_tasks(const fast::msg::migfra::Task_container &task_cont) override;
};

#endif
