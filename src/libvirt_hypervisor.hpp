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
