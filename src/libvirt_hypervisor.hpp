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
	void start(const std::string &vm_name, size_t vcpus, size_t memory);
	void stop(const std::string &vm_name);
	void migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration);
private:
	void start_task();
	void stop_task();
	void migrate_task();

	virConnectPtr local_host_conn;	
	unsigned int thread_counter;
	std::mutex thread_counter_mutex;
	std::condition_variable thread_counter_cv;
};

#endif
