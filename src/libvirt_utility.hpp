#ifndef LIBVIRT_UTILITY_HPP
#define LIBVIRT_UTILITY_HPP

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

#endif
