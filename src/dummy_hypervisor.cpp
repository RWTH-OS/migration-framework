/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "dummy_hypervisor.hpp"

#include <stdexcept>

Dummy_hypervisor::Dummy_hypervisor(bool never_throw) noexcept :
	never_throw(never_throw)
{
}

void Dummy_hypervisor::start(const std::string &vm_name, unsigned int vcpus, unsigned long memory)
{
	(void) vm_name; (void) vcpus; (void) memory;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::stop(const std::string &vm_name) 
{
	(void) vm_name;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool rdma_migration)
{
	(void) vm_name; (void) dest_hostname; (void) live_migration; (void) rdma_migration;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}
