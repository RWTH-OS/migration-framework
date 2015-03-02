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
