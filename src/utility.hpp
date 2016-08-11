#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <libvirt/libvirt.h>

#include <string>

//
// Some deleter to be used with smart pointers.
//

struct Deleter_virConnect
{
	void operator()(virConnectPtr ptr) const
	{
		virConnectClose(ptr);
	}
};

struct Deleter_virDomain
{
	void operator()(virDomainPtr ptr) const
	{
		virDomainFree(ptr);
	}
};

struct Deleter_virDomainSnapshot
{
	void operator()(virDomainSnapshotPtr ptr) const
	{
		virDomainSnapshotFree(ptr);
	}
};

// Libvirt sometimes returns a dynamically allocated cstring.
// As we prefer std::string this function converts and frees.
std::string convert_and_free_cstr(char *cstr);

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

#endif
