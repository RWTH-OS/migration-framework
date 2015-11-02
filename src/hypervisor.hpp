/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef HYPERVISOR_HPP
#define HYPERVISOR_HPP

#include "pci_device_handler.hpp"

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
	virtual void start(const std::string &vm_name, unsigned int vcpus, unsigned long memory, const std::vector<PCI_id> &pci_ids) = 0;
	/**
	 * \brief Method to stop a virtual machine.
	 *
	 * A pure virtual method to provide an interface for stopping a virtual machine.
	 * \param vm_name The name of the vm to stop.
	 */
	virtual void stop(const std::string &vm_name) = 0;
	/**
	 * \brief Method to migrate a virtual machine to another host.
	 *
	 * A pure virtual method to provide an interface for migrating a virtual machine to another host.
	 * \param vm_name The name of the vm to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Enables live migration.
	 * \param rdma_migration Enables rdma migration.
	 */
	virtual void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool rdma_migration) = 0;
};

#endif
