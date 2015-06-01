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
#include <mutex>
#include <condition_variable>

class Libvirt_hypervisor :
	public Hypervisor
{
public:
	/// TODO: Consider copy/move constructor implementation.
	Libvirt_hypervisor();
	~Libvirt_hypervisor();
	void start(const std::string &vm_name, unsigned int vcpus, unsigned long memory);
	void stop(const std::string &vm_name);
	void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration);
private:
	virConnectPtr local_host_conn;	
};

#endif
