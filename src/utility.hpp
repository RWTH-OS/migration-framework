#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <libvirt/libvirt.h>

#include <string>
#include <vector>

//
// Some deleter to be used with smart pointers.
//

struct Deleter_virConnect
{
	void operator()(virConnectPtr ptr) const
	{
		if (ptr)
			virConnectClose(ptr);
	}
};

struct Deleter_virDomain
{
	void operator()(virDomainPtr ptr) const
	{
		if (ptr)
			virDomainFree(ptr);
	}
};

struct Deleter_virDomainSnapshot
{
	void operator()(virDomainSnapshotPtr ptr) const
	{
		if (ptr)
			virDomainSnapshotFree(ptr);
	}
};

// Libvirt sometimes returns a dynamically allocated cstring.
// As we prefer std::string this function converts and frees.
std::string convert_and_free_cstr(char *cstr);

// Get an xml string of the domains config
std::string get_domain_xml(virDomainPtr domain);

// Struct for holding memory stats of a domain.
struct Memory_stats
{
	Memory_stats(virDomainPtr domain);

	std::string str() const;
	void refresh();

	unsigned long long unused = 0;
	unsigned long long available = 0;
	unsigned long long actual_balloon = 0;
	virDomainPtr domain = nullptr;
};

// Get memory size in KiB
unsigned long long get_memory_size(virDomainPtr domain);

// Get hostname
std::string get_hostname();

// Suspend domain
void suspend_domain(virDomainPtr domain);

// Resume domain
void resume_domain(virDomainPtr domain);

// Repinning the vcpus to cpus
void repin_vcpus(virDomainPtr domain, const std::vector<std::vector<unsigned int>> &vcpu_map);



#endif
