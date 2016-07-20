#ifndef LIBVIRT_UTILITY_HPP
#define LIBVIRT_UTILITY_HPP

#include <libvirt/libvirt.h>

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

#endif
