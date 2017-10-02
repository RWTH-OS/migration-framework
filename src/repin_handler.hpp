/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef REPIN_HANDLER_HPP
#define REPIN_HANDLER_HPP

#include <fast-lib/optional.hpp>
#include <fast-lib/message/migfra/time_measurement.hpp>

#include <libvirt/libvirt.h>

#include <memory>
#include <vector>
#include <string>

// RAII-guard to add pause option to migrate flags in constructor and repin and resume in destructor.
// If no error occures during migration the domain on destination should be set.
class Repin_guard
{
public:
	Repin_guard(std::shared_ptr<virDomain> domain,
			unsigned long &flags,
			const fast::Optional<std::vector<std::vector<unsigned int>>> &vcpu_map,
			fast::msg::migfra::Time_measurement &time_measurement,
			std::string tag_postfix = "");
	~Repin_guard() noexcept(false);

	void set_destination_domain(std::shared_ptr<virDomain> dest_domain);
	void repin();
private:
	std::shared_ptr<virDomain> domain;
	unsigned long &flags;
	const fast::Optional<std::vector<std::vector<unsigned int>>> &vcpu_map;
	fast::msg::migfra::Time_measurement &time_measurement;
	std::string tag_postfix;

};

#endif
