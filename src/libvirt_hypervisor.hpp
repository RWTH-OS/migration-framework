#ifndef LIBVIRT_HYPERVISOR_HPP
#define LIBVIRT_HYPERVISOR_HPP

#include "hypervisor.hpp"

class Libvirt_hypervisor :
	public Hypervisor
{
public:
	void start(const std::string &vm_name, size_t vcpus, size_t memory);
	void stop(const std::string &vm_name);
	void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration);
};

#endif
