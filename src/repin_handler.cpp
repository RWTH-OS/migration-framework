#include "repin_handler.hpp"

#include "utility.hpp"

#include <fast-lib/log.hpp>

using namespace fast::msg::migfra;

FASTLIB_LOG_INIT(repin_guard_log, "Repin_guard")
FASTLIB_LOG_SET_LEVEL_GLOBAL(repin_guard_log, trace);

//
// Repin_guard implementation
//

Repin_guard::Repin_guard(std::shared_ptr<virDomain> domain, 
		unsigned long &flags, 
		const fast::Optional<std::vector<std::vector<unsigned int>>> &vcpu_map,
		Time_measurement &time_measurement, 
		std::string tag_postfix) :
	domain(domain),
	flags(flags),
	vcpu_map(vcpu_map),
	time_measurement(time_measurement),
	tag_postfix(std::move(tag_postfix))
{
	if (this->tag_postfix != "")
		this->tag_postfix = "-" + this->tag_postfix;
	if (vcpu_map.is_valid()) {
		FASTLIB_LOG(repin_guard_log, trace) << "Setting paused-after-migration flag for repinning.";
		flags |= VIR_MIGRATE_PAUSED;
	}
}

Repin_guard::~Repin_guard() noexcept(false)
{
	try {
		repin();
	} catch (...) {
		// Only log exception when unwinding stack, else rethrow exception.
		if (std::uncaught_exception())
			FASTLIB_LOG(repin_guard_log, trace) << "Exception while repinning/resuming.";
		else
			throw;
	}
}

void Repin_guard::set_destination_domain(std::shared_ptr<virDomain> dest_domain)
{
	// override domain to reattach devices on
	domain = dest_domain;
}

void Repin_guard::repin()
{
	if (vcpu_map.is_valid()) {
		time_measurement.tick("repin" + tag_postfix);
		if (!std::uncaught_exception()) {
			FASTLIB_LOG(repin_guard_log, trace) << "Repin vcpus.";
			repin_vcpus(domain.get(), vcpu_map.get());
		}
		resume_domain(domain.get());
		time_measurement.tock("repin" + tag_postfix);
	}
}

