#include "utility.hpp"

#include <libvirt/virterror.h>

#include <climits>
#include <cstring>
#include <unistd.h>
#include <stdexcept>

// TODO: Consider using utility namespace and splitting the file

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
	char hostname_cstr[HOST_NAME_MAX];
	int ret;
	if ((ret = gethostname(hostname_cstr, HOST_NAME_MAX)) != 0)
		std::runtime_error(std::string("Failed getting hostname: ") + std::strerror(ret));
	const std::string hostname(hostname_cstr, std::strlen(hostname_cstr));
	return hostname;
}

void suspend_domain(virDomainPtr domain)
{
	if (virDomainSuspend(domain) == -1)
		throw std::runtime_error(std::string("Error suspending domain: ") + virGetLastErrorMessage());
}

void resume_domain(virDomainPtr domain)
{
	if (virDomainResume(domain) == -1)
		throw std::runtime_error(std::string("Error resuming domain: ") + virGetLastErrorMessage());
}

virConnectPtr get_connect_of_domain(virDomainPtr domain)
{
	auto ptr = virDomainGetConnect(domain);
	if (ptr == nullptr)
		throw std::runtime_error(std::string("Error getting connection of domain: ") + virGetLastErrorMessage());
	return ptr;
}

size_t get_cpumaplen(virConnectPtr conn)
{
	auto cpus = virNodeGetCPUMap(conn, nullptr, nullptr, 0);
	if (cpus == -1)
		throw std::runtime_error(std::string("Error getting number of CPUs: ") + virGetLastErrorMessage());
	return VIR_CPU_MAPLEN(cpus);
}

void pin_vcpu_to_cpus(virDomainPtr domain, unsigned int vcpu, std::vector<unsigned int> cpus, size_t maplen)
{
	std::vector<unsigned char> cpumap(maplen, 0);
	for (auto cpu : cpus)
		VIR_USE_CPU(cpumap, cpu);
	if (virDomainPinVcpuFlags(domain, vcpu, cpumap.data(), maplen, VIR_DOMAIN_AFFECT_CURRENT) == -1)
		throw std::runtime_error(std::string("Error pinning vcpu: ") + virGetLastErrorMessage());
}

void repin_vcpus(virDomainPtr domain, const std::vector<std::vector<unsigned int>> &vcpu_map)
{
	// Get number of CPUs on node
	auto maplen = get_cpumaplen(get_connect_of_domain(domain));
	// Create cpumap and pin for each vcpu
	for (unsigned int vcpu = 0; vcpu != vcpu_map.size(); ++vcpu) {
		pin_vcpu_to_cpus(domain, vcpu, vcpu_map[vcpu], maplen);
	}
}
