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

std::string get_domain_xml(virDomainPtr domain)
{
	auto xml_str = convert_and_free_cstr(virDomainGetXMLDesc(domain, 0));
	if (xml_str == "")
		throw std::runtime_error("Error getting xml description.");
	return xml_str;
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

unsigned long long get_memory_size(virDomainPtr domain)
{
	Memory_stats mem_stats(domain);
	return mem_stats.actual_balloon;
}

std::string get_hostname()
{
	char hostname_cstr[_POSIX_HOST_NAME_MAX];
	int ret;
	if ((ret = gethostname(hostname_cstr, _POSIX_HOST_NAME_MAX)) != 0)
		std::runtime_error(std::string("Failed getting hostname: ") + std::strerror(ret));
	const std::string hostname(hostname_cstr, std::strlen(hostname_cstr));
	return hostname;
}
