/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "dummy_hypervisor.hpp"

void Dummy_hypervisor::start(const std::string &vm_name, unsigned int vcpus, unsigned long memory) noexcept
{
	(void) vm_name; (void) vcpus; (void) memory;
}

void Dummy_hypervisor::stop(const std::string &vm_name) noexcept
{
	(void) vm_name;
}

void Dummy_hypervisor::migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration) noexcept
{
	(void) vm_name; (void) dest_hostname; (void) live_migration;
}
