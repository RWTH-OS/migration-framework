#ifndef HYPERVISOR_HPP
#define HYPERVISOR_HPP

#include <string>

class Hypervisor
{
public:
	virtual ~Hypervisor() {};
	virtual void start(const std::string &vm_name, size_t vcpus, size_t memory) = 0;
	virtual void stop(const std::string &vm_name) = 0;
	virtual void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration) = 0;
};

#endif
