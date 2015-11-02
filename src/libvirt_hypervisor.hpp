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

#include <libvirt/libvirt.h>

#include <memory>
#include <vector>

class PCI_device_handler;

/**
 * \brief Implementation of the Hypervisor interface using libvirt API.
 *
 * This class implements the Hypervisor interface.
 * It provides methods to start, stop and migrate virtual machines.
 * \todo: Consider adding copy/move constructor implementation.
 */
class Libvirt_hypervisor :
	public Hypervisor
{
public:
	/**
	 * \brief Constructor for Libvirt_hypervisor.
	 *
	 * Establishes an connection to qemu on the local host.
	 */
	Libvirt_hypervisor();
	/**
	 * \brief Destructor for Libvirt_hypervisor.
	 *
	 * Disconnects from qemu.
	 */
	~Libvirt_hypervisor();
	/**
	 * \brief Method to start a virtual machine.
	 *
	 * Calls libvirt API to start a virtual machine.
	 * \param vm_name The name of the vm to start.
	 * \param vcpus The number of virtual cpus to be assigned to the vm.
	 * \param memory The amount of ram memory to be assigned to the vm in KiB.
	 */
	void start(const std::string &vm_name, unsigned int vcpus, unsigned long memory, const std::vector<PCI_id> &pci_ids);
	/**
	 * \brief Method to stop a virtual machine.
	 *
	 * Calls libvirt API to stop a virtual machine.
	 * \param vm_name The name of the vm to stop.
	 */
	void stop(const std::string &vm_name);
	/**
	 * \brief Method to migrate a virtual machine to another host.
	 *
	 * Calls libvirt API to migrate a virtual machine to another host.
	 * SSH access to the destination host must be available.
	 * \param vm_name The name of the vm to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Enables live migration.
	 * \param rdma_migration Enables rdma migration.
	 */
	void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool rdma_migration);
private:
	virConnectPtr local_host_conn;	
	std::shared_ptr<PCI_device_handler> pci_device_handler;
};

#endif
