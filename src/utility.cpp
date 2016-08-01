#include "utility.hpp"

#include <climits>
#include <cstring>
#include <unistd.h>
#include <stdexcept>

std::string convert_and_free_cstr(char *cstr)
{
	std::string str;
	if (cstr) {
		str.assign(cstr);
		free(cstr);
	}
	return str;
}

Memory_stats::Memory_stats(virDomainPtr domain) :
	domain(domain)
{
	refresh();
}

std::string Memory_stats::str() const
{
	return "Unused: " + std::to_string(unused) + ", available: " + std::to_string(available) + ", actual: " + std::to_string(actual_balloon);
}
void Memory_stats::refresh()
{
	virDomainMemoryStatStruct mem_stats[VIR_DOMAIN_MEMORY_STAT_NR];
	int statcnt;
	if ((statcnt = virDomainMemoryStats(domain, mem_stats, VIR_DOMAIN_MEMORY_STAT_RSS, 0)) == -1)
		throw std::runtime_error("Error getting memory stats");
	for (int i = 0; i != statcnt; ++i) {
		if (mem_stats[i].tag == VIR_DOMAIN_MEMORY_STAT_UNUSED)
			unused = mem_stats[i].val;
		if (mem_stats[i].tag == VIR_DOMAIN_MEMORY_STAT_AVAILABLE)
			available = mem_stats[i].val;
		if (mem_stats[i].tag == VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON)
			actual_balloon = mem_stats[i].val;
	}
}

std::string get_hostname()
{
	char hostname_cstr[HOST_NAME_MAX];
	int ret;
	if ((ret = gethostname(hostname_cstr, HOST_NAME_MAX)) != 0)
		std::runtime_error(std::string("Failed getting hostname: ") + std::strerror(ret));
	const std::string hostname(hostname_cstr, std::strlen(hostname_cstr));
	return hostname;
}
