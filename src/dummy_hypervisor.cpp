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

void Dummy_hypervisor::start(const fast::msg::migfra::Start &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) task; (void) time_measurement;;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::stop(const fast::msg::migfra::Stop &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) task; (void) time_measurement;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::migrate(const fast::msg::migfra::Migrate &task, fast::msg::migfra::Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm)
{
	(void) task; (void) time_measurement; (void) comm;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::repin(const fast::msg::migfra::Repin &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) task; (void) time_measurement;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) task; (void) time_measurement;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}

void Dummy_hypervisor::resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) task; (void) time_measurement;
	if (!never_throw)
		throw std::runtime_error("Dummy_hypervisor is set to throw always if called.");
}
