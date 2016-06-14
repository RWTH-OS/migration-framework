/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef LIBVIRT_HYPERVISOR_HPP
#define LIBVIRT_HYPERVISOR_HPP

#include "hypervisor.hpp"

#include <memory>
#include <vector>
#include <string>

class PCI_device_handler;

/**
 * \brief Implementation of the Hypervisor interface using libvirt API.
 *
 * This class implements the Hypervisor interface.
 * It provides methods to start, stop and migrate virtual machines.
 * \todo: Consider adding copy/move constructor implementation.
 * \todo: Fix documentation of parameters.
 */
class Libvirt_hypervisor :
	public Hypervisor
{
public:
	/**
	 * \brief Constructor for Libvirt_hypervisor.
	 *
	 * Establishes an connection to qemu on the local host.
	 * \param nodes Defines the nodes to look for already running virtual machines.
	 */
	Libvirt_hypervisor(std::vector<std::string> nodes, std::string default_driver, std::string default_transport);
	/**
	 * \brief Method to start a virtual machine.
	 *
	 * Calls libvirt API to start a virtual machine.
	 * \param vm_name The name of the vm to start.
	 * \param vcpus The number of virtual cpus to be assigned to the vm.
	 * \param memory The amount of ram memory to be assigned to the vm in KiB.
	 */
	void start(const fast::msg::migfra::Start &task, fast::msg::migfra::Time_measurement &time_measurement);
	/**
	 * \brief Method to stop a virtual machine.
	 *
	 * Calls libvirt API to stop a virtual machine.
	 * \param vm_name The name of the vm to stop.
	 * \param force If true the domain is destroyed, else it is shut down gracefully.
	 */
	void stop(const fast::msg::migfra::Stop &task, fast::msg::migfra::Time_measurement &time_measurement);
	/**
	 * \brief Method to migrate a virtual machine to another host.
	 *
	 * Calls libvirt API to migrate a virtual machine to another host.
	 * SSH access to the destination host must be available.
	 * \param vm_name The name of the vm to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Enables live migration.
	 * \param rdma_migration Enables rdma migration.
	 * \param time_measurement Time measurement facility.
	 */
	void migrate(const fast::msg::migfra::Migrate &task, fast::msg::migfra::Time_measurement &time_measurement);
private:
	std::shared_ptr<PCI_device_handler> pci_device_handler;
	std::vector<std::string> nodes;
	std::string default_driver;
	std::string default_transport;
};

#endif
