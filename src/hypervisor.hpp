/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef HYPERVISOR_HPP
#define HYPERVISOR_HPP

#include <string>

/**
 * \brief An abstract class to provide an interface for the hypervisor.
 *
 * This interface provides methods to start, stop and migrate virtual machines.
 */
class Hypervisor
{
public:
	virtual ~Hypervisor() {};
	virtual void start(const std::string &vm_name, unsigned int vcpus, unsigned long memory) = 0;
	virtual void stop(const std::string &vm_name) = 0;
	virtual void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration) = 0;
};

#endif
