/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "libvirt_hypervisor.hpp"

#include "pscom_handler.hpp"
#include "pci_device_handler.hpp"
#include "utility.hpp"
#include "ivshmem_handler.hpp"
#include "repin_handler.hpp"

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <fast-lib/log.hpp>
#include <libssh/libsshpp.hpp>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdexcept>
#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <mutex>
#include <regex>
#include <functional>

using namespace fast::msg::migfra;

FASTLIB_LOG_INIT(libvirt_hyp_log, "Libvirt_hypervisor")
FASTLIB_LOG_SET_LEVEL_GLOBAL(libvirt_hyp_log, trace);

//
// Helper functions
//

/**
 * \brief Tries to connect to a domain via ssh in order to test if the domain is booted and ready to use.
 *
 * \param domain The domain to probe.
 */
void probe_ssh_connection(const std::string &host, const std::chrono::duration<double> &timeout)
{
	ssh::Session session;
	session.setOption(SSH_OPTIONS_HOST, host.c_str());
	bool success = false;
	auto start = std::chrono::high_resolution_clock::now();
	do {
		try {
			FASTLIB_LOG(libvirt_hyp_log, trace) << "Try to connect to domain (" << host << ") with SSH.";
			session.connect();
			FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain is ready.";
			success = true;
		} catch (ssh::SshException &e) {
			FASTLIB_LOG(libvirt_hyp_log, debug) << "Exception while connecting to " << host << " with SSH: " << e.getError();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		session.disconnect();
		if (!success && std::chrono::high_resolution_clock::now() - start > timeout)
			throw std::runtime_error("Timeout while trying to reach domain with SSH.");
	} while (!success);
}

/**
 * \brief Get a libvirt-connection to a specific host and libvirt-driver.
 *
 * \param host The hostname of the connection.
 * \param driver The libvirt-driver of the connection (e.g., qemu).
 * \param transport The transport protocol to use (e.g., ssh or tcp for remote connections)
 * \returns shared_ptr to a domain.
 */
std::shared_ptr<virConnect> connect(const std::string &host, const std::string &driver, const std::string transport = "")
{
	std::string plus_transport = (transport != "") ? ("+" + transport) : "";
	std::string mode = (driver == "lxctools") ? "" : "system";
	std::string uri = driver + plus_transport + "://" + host + "/" + mode;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Connect to " + uri;
	std::shared_ptr<virConnect> conn(
			virConnectOpen(uri.c_str()),
			Deleter_virConnect()
	);
	if (!conn)
		throw std::runtime_error("Failed to connect to libvirt with uri: " + uri);
	return conn;
}

std::string get_domain_name(virDomainPtr domain)
{
	auto ret = virDomainGetName(domain);
	if (!ret)
		throw std::runtime_error(std::string("Error getting name of domain.") + virGetLastErrorMessage());
	return std::string(ret);

}

// TODO: If this function throws there is a memory leak due to use of free().
std::vector<std::string> get_active_domain_names(virConnectPtr conn)
{
	virDomainPtr *domains;
	auto num = virConnectListAllDomains(conn, &domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
	if (num < 0)
		throw std::runtime_error(std::string("Error getting list of active domains.") + virGetLastErrorMessage());
	std::vector<std::string> vm_names;
	vm_names.reserve(num);
	for (int i = 0; i != num; ++i) {
		vm_names.push_back(get_domain_name(domains[i])); // possible memory leak if get_domain_name throws :(
		virDomainFree(domains[i]);
	}
	free(domains);
	return vm_names;
}

/**
 * \brief Returns the number of CPUs for a host.
 * TODO: Check for actually online CPUs
 */
int get_host_cpu_count(virConnectPtr conn)
{
	int cpu_count = -1;
	if ((cpu_count = virNodeGetCPUMap(conn, nullptr, nullptr, 0)) == -1)
		throw std::runtime_error("Error getting node ");
	return cpu_count;
}


/**
 * \brief Define a domain using an xml config.
 *
 * This function only defines the domain but does not start it.
 * \param conn The connection used to define the domain on.
 * \param xml The xml configuration of the domain.
 * \returns shared_ptr to a domain.
 */
std::shared_ptr<virDomain> define_from_xml(virConnectPtr conn, const std::string &xml)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Define persistant domain from xml";
	std::shared_ptr<virDomain> domain(
			virDomainDefineXML(conn, xml.c_str()),
			Deleter_virDomain()
	);
	if (!domain)
		throw std::runtime_error(std::string("Error defining domain from xml.") + virGetLastErrorMessage());
	return domain;
}

/**
 * \brief Create a domain using an xml config.
 *
 * This function only starts the described domain but does not define a persistent domain.
 * \param conn The connection used to start the domain on.
 * \param xml The xml configuration of the domain.
 * \returns shared_ptr to a domain.
 */
std::shared_ptr<virDomain> create_from_xml(virConnectPtr conn, const std::string &xml, bool paused = false)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Create domain from xml";
	std::shared_ptr<virDomain> domain(
			virDomainCreateXML(conn, xml.c_str(), paused ? VIR_DOMAIN_START_PAUSED : VIR_DOMAIN_NONE),
			Deleter_virDomain()
	);
	if (!domain)
		throw std::runtime_error(std::string("Error creating domain from xml.") + virGetLastErrorMessage());
	return domain;
}

/**
 * \brief Find a domain with the specified name.
 *
 * \param conn The connection to search for the domain on.
 * \param name The name of the domain.
 */
std::shared_ptr<virDomain> find_by_name(virConnectPtr conn, const std::string &name)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Get domain by name.";
	std::shared_ptr<virDomain> domain(
		virDomainLookupByName(conn, name.c_str()),
		Deleter_virDomain()
	);
	if (!domain)
		throw std::runtime_error(virGetLastErrorMessage());
	return domain;
}

/**
 * \brief Start a domain.
 *
 * \param domain The domain to start.
 */
void create(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Create domain.";
	if (virDomainCreate(domain) == -1)
		throw std::runtime_error(std::string("Error creating domain: ") + virGetLastErrorMessage());
}

/**
 * \brief Get the state of the domain.
 *
 * \param domain The domain which state is retrieved.
 * \returns One of enum virDomainState (http://libvirt.org/html/libvirt-libvirt-domain.html#virDomainState).
 */
unsigned char get_domain_state(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Get domain info.";
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain, &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	return domain_info.state;
}

/**
 * \brief Custom exception thrown when domain state does not suit expected state.
 */
struct Domain_state_error :
	public std::runtime_error
{
	explicit Domain_state_error(const std::string &what_arg) :
		std::runtime_error(what_arg)
	{
	}
};

/**
 * \brief Check if state of a domain is as expected.
 *
 * \param domain The domain to check the state of.
 * \param expected_state The expected state with which the state of the domain is compared to.
 */
void check_state(virDomainPtr domain, virDomainState expected_state)
{
	auto state = get_domain_state(domain);
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Check domain state.";
	if (state != expected_state)
		throw Domain_state_error("Wrong domain state: " + std::to_string(state));
}

/**
 * \brief Check if state of a domain is as expected on at least one of the nodes.
 *
 * This function is used to check if a domain is already running on any node.
 * \param name The name of the domain.
 * \param nodes The nodes to look for the domain.
 * \param expected_state The expected state with which the state of the domain is compared to.
 */
void check_remote_state(const std::string &name, const std::vector<std::string> &nodes, virDomainState expected_state)
{
	for (const auto &node : nodes) {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Check domain state on " + node + ".";
		auto conn = connect(node, "qemu", "ssh");
		try {
			auto domain = find_by_name(conn.get(), name);
			check_state(domain.get(), expected_state);
		} catch (const Domain_state_error &e) {
			throw std::runtime_error("Domain already running on " + node);
		} catch (const std::runtime_error &e) {
			if (e.what() != std::string("Domain not found."))
				throw;
		}
	}
}

/**
 * \brief Wait until the domain is in a specific state.
 *
 * This function may be used to wait until a domain is activated or fully shut down.
 * \param domain The domain to poll.
 * \param expected_state The state to wait on.
 * \todo Implement timeout
 */
void wait_for_state(virDomainPtr domain, virDomainState expected_state, const std::chrono::duration<double> timeout)
{
	auto start = std::chrono::high_resolution_clock::now();
	while (get_domain_state(domain) != expected_state) {
		if (std::chrono::high_resolution_clock::now() - start > timeout)
			throw std::runtime_error("Timeout while waiting for correct vm state.");
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

bool is_persistent(virDomainPtr domain)
{
	auto ret = virDomainIsPersistent(domain);
	if (ret == -1)
		throw std::runtime_error(std::string("Error checking if domain is persistent: ") + virGetLastErrorMessage());
	return ret;
}

void set_memory(virDomainPtr domain, unsigned long memory)
{
	if (virDomainSetMemoryFlags(domain, memory, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting amount of memory to " + std::to_string(memory)
				+ " KiB.");
}

void set_max_memory(virDomainPtr domain, unsigned long memory)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Set memory.";
	if (virDomainSetMemoryFlags(domain, memory, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_MEM_MAXIMUM) == -1) {
		throw std::runtime_error("Error setting maximum amount of memory to " + std::to_string(memory)
				+ " KiB.");
	}
}

void set_max_vcpus(virDomainPtr domain, unsigned int vcpus)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Set VCPUs.";
	if (virDomainSetVcpusFlags(domain, vcpus, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_VCPU_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum number of vcpus to " + std::to_string(vcpus)
				+ ".");
}

void set_vcpus(virDomainPtr domain, unsigned int vcpus)
{
	if (virDomainSetVcpusFlags(domain, vcpus, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting number of vcpus to " + std::to_string(vcpus)
				+ ".");
}

void destroy(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Destroy domain.";
	if (virDomainDestroy(domain) == -1)
		throw std::runtime_error(std::string("Error destroying domain: ") + virGetLastErrorMessage());
}

void delete_snapshot(virDomainSnapshotPtr snapshot, bool metadata_only = false)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Delete snapshot.";
	if (virDomainSnapshotDelete(snapshot, metadata_only ? VIR_DOMAIN_SNAPSHOT_DELETE_METADATA_ONLY : 0) == -1)
		throw std::runtime_error(std::string("Error deleting snapshot: ") + virGetLastErrorMessage());
}

void revert_to_snapshot(virDomainSnapshotPtr snapshot, bool paused)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Revert to snapshot.";
	if (virDomainRevertToSnapshot(snapshot, paused ? VIR_DOMAIN_SNAPSHOT_REVERT_PAUSED : VIR_DOMAIN_SNAPSHOT_REVERT_RUNNING) == -1)
		throw std::runtime_error(std::string("Error reverting snapshot: ") + virGetLastErrorMessage());
}

std::shared_ptr<virDomainSnapshot> create_snapshot(virDomainPtr domain, bool halt)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Create snapshot";
	std::shared_ptr<virDomainSnapshot> snapshot(
			virDomainSnapshotCreateXML(domain,
"<domainsnapshot><description>Snapshot for migration</description>\
	<memory snapshot='internal'/>\
</domainsnapshot>"
			, halt ? VIR_DOMAIN_SNAPSHOT_CREATE_HALT : 0),
			Deleter_virDomainSnapshot()
	);
	if (!snapshot)
		throw std::runtime_error("Error creating snapshot.");
	return snapshot;
}

std::shared_ptr<virDomainSnapshot> redefine_snapshot(virDomainPtr domain, virDomainSnapshotPtr snapshot)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Redefine snapshot on remote";
	auto xml = convert_and_free_cstr(virDomainSnapshotGetXMLDesc(snapshot, VIR_DOMAIN_XML_SECURE));
	std::shared_ptr<virDomainSnapshot> dest_snapshot(
			virDomainSnapshotCreateXML(domain, xml.c_str(), VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE),
			Deleter_virDomainSnapshot()
	);
	return dest_snapshot;
}

std::string get_migrate_uri(bool rdma_migration, const std::string &dest_hostname)
{
	std::string migrate_uri = rdma_migration ? "rdma://" + dest_hostname + "-ib" : "";
	FASTLIB_LOG(libvirt_hyp_log, trace) << (rdma_migration ? "Use migrate uri: " + migrate_uri + "." : "Use default migrate uri.");
	return migrate_uri;
}

unsigned long get_migrate_flags(std::string migration_type)
{
	unsigned long flags = 0;
	if (migration_type == "live") {
		flags |= VIR_MIGRATE_LIVE;
	} else if (migration_type == "offline") {
		flags |= VIR_MIGRATE_OFFLINE;
	} else if (migration_type != "warm") {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Unknown migration type " << migration_type << ".";
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Using warm migration as fallback.";
	}
	return flags;
}

std::shared_ptr<virDomain> migrate_domain(virDomainPtr domain, virConnectPtr dest_conn, unsigned long flags, const std::string &migrate_uri)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Migrate domain.";
	// Create params only containing migrate uri.
	std::unique_ptr<virTypedParameter[]> params;
	size_t params_size = 0;
	if (migrate_uri != "") {
		params.reset(new virTypedParameter[1]);
		strcpy(params[0].field, VIR_MIGRATE_PARAM_URI);
		params[0].type = VIR_TYPED_PARAM_STRING;
		// TODO: Fix memory leak
		params[0].value.s = new char[migrate_uri.size() + 1];
		strcpy(params[0].value.s, migrate_uri.c_str());
		params_size = 1;
	}
	// Migrate
	std::shared_ptr<virDomain> dest_domain(
		virDomainMigrate3(domain, dest_conn, params.get(), params_size, flags),
		Deleter_virDomain()
	);
	// Check for error
	if (!dest_domain)
		throw std::runtime_error(std::string("Migration failed: ") + virGetLastErrorMessage());
	return dest_domain;
}

bool sort_domains_by_size(virDomainPtr domain1, virDomainPtr domain2)
{
	Memory_stats mem_stats1(domain1);
	Memory_stats mem_stats2(domain2);
	auto domain1_size = mem_stats1.actual_balloon;
	auto domain2_size = mem_stats2.actual_balloon;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain1 size: " << domain1_size;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain2 size: " << domain2_size;
	return domain1_size < domain2_size;
}

unsigned long long get_free_memory(virConnectPtr conn)
{
	auto memory = virNodeGetFreeMemory(conn);
	if (memory == 0)
		throw std::runtime_error(std::string("Error geting free node memory: ") + virGetLastErrorMessage());
	return memory;
}

bool check_snapshot_required(virDomainPtr domain1, virConnectPtr conn1, virDomainPtr domain2, virConnectPtr conn2)
{
	auto domain1_size = get_memory_size(domain1);
	auto domain2_size = get_memory_size(domain2);
	auto host1_free_memory = get_free_memory(conn1);
	auto host2_free_memory = get_free_memory(conn2);
	return host1_free_memory < domain2_size || host2_free_memory < domain1_size;
}

// TODO: Refactor (maybe object oriented approach?)
void Libvirt_hypervisor::swap_migration(const std::string &name, const std::string &name_swap, const std::string &hostname, const std::string &hostname_swap, unsigned long flags, unsigned long flags_swap, bool rdma_migration, const std::string &driver, const std::string &transport, const Migrate &task, std::shared_ptr<fast::Communicator> comm, Time_measurement &time_measurement)
{
	auto conn = connect(hostname, driver, transport);
	auto conn_swap = connect(hostname_swap, driver, transport);
	auto domain = find_by_name(conn.get(), name);
	auto domain_swap = find_by_name(conn_swap.get(), name_swap);
	// Get domains
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Swap " << name << " with " << name_swap << ".";
	// Check if domains are in running state
	check_state(domain.get(), VIR_DOMAIN_RUNNING);
	check_state(domain_swap.get(), VIR_DOMAIN_RUNNING);
	// Suspend pscom (resume in destructor)
	Pscom_handler pscom_handler(task, comm, time_measurement, false);
	Pscom_handler pscom_handler_swap(task, comm, time_measurement, true);
	// Guard migration of PCI devices.
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Create guards for device migration.";
	Migrate_devices_guard dev_guard(pci_device_handler, domain, time_measurement, name);
	Migrate_devices_guard dev_guard_swap(pci_device_handler, domain_swap, time_measurement, name_swap);
	Migrate_ivshmem_guard ivshmem_guard(domain, time_measurement, name);
	Migrate_ivshmem_guard ivshmem_guard_swap(domain_swap, time_measurement, name_swap);
	// Guard repin of vcpus.
	// In particular, resume after migration since repin is done after migration in suspended state.
	Repin_guard repin_guard(domain, flags, task.vcpu_map, time_measurement, name);
	Repin_guard repin_guard_swap(domain_swap, flags_swap, task.swap_with.get().vcpu_map, time_measurement, name_swap);
	// Compare size and snapshot-swap if necessary
	if (check_snapshot_required(domain.get(), conn.get(), domain_swap.get(), conn_swap.get())) {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Starting swap-migration using snapshot.";
		// TODO: RAII handler for snapshot for better error recovery
		// TODO: Move to dedicated function
		auto func = [=, &time_measurement](decltype(domain) domain1, decltype(name) name1, decltype(conn) conn1, decltype(hostname) hostname1, decltype(flags) flags1, decltype(dev_guard) &dev_guard1, decltype(ivshmem_guard) &ivshmem_guard1, decltype(repin_guard) &repin_guard1,
				decltype(domain) domain2, decltype(name) name2, decltype(conn) conn2, decltype(flags) flags2, decltype(dev_guard) &dev_guard2, decltype(ivshmem_guard) &ivshmem_guard2, decltype(repin_guard) &repin_guard2)
		{
			// Suspend vm1
			time_measurement.tick("downtime-" + name1);
			time_measurement.tick("suspend-" + name1);
			// Take snapshot of vm1 and halt.
			auto snapshot = create_snapshot(domain1.get(), true);
			time_measurement.tock("suspend-" + name1);
			// Create migrateuri for vm2
			std::string migrate_uri = get_migrate_uri(rdma_migration, hostname1);
			// Migrate vm2
			time_measurement.tick("migrate-" + name2);
			auto dest_domain2 = migrate_domain(domain2.get(), conn1.get(), flags2, migrate_uri);
			time_measurement.tock("migrate-" + name2);
			// Set destination domain for guard of vm2
			repin_guard2.set_destination_domain(dest_domain2);
			dev_guard2.set_destination_domain(dest_domain2);
			ivshmem_guard2.set_destination_domain(dest_domain2);
			// Get snapshotted domain on dest
			time_measurement.tick("resume-" + name1);
			// TODO: handle transient domains
			auto dest_domain1 = find_by_name(conn2.get(), name1);
			// Redefine snapshot on remote
			auto dest_snapshot = redefine_snapshot(dest_domain1.get(), snapshot.get());
			// Remove snapshot from src
			delete_snapshot(snapshot.get(), true);
			// Revert to snapshot (paused if repin is required)
			revert_to_snapshot(dest_snapshot.get(), flags1 & VIR_MIGRATE_PAUSED);
			time_measurement.tock("resume-" + name1);
			time_measurement.tock("downtime-" + name1);
			// Remove snapshot from destination
			delete_snapshot(dest_snapshot.get());
			// Set destination domain for guard
			repin_guard1.set_destination_domain(dest_domain1);
			dev_guard1.set_destination_domain(dest_domain1);
			ivshmem_guard1.set_destination_domain(dest_domain1);
		};
		if (sort_domains_by_size(domain.get(), domain_swap.get()))
			func(domain, name, conn, hostname, flags, dev_guard, ivshmem_guard, repin_guard, domain_swap, name_swap, conn_swap, flags_swap, dev_guard_swap, ivshmem_guard_swap, repin_guard_swap);
		else
			func(domain_swap, name_swap, conn_swap, hostname_swap, flags_swap, dev_guard_swap, ivshmem_guard_swap, repin_guard_swap, domain, name, conn, flags, dev_guard, ivshmem_guard, repin_guard);
	} else {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Starting swap-migration using parallel migration.";
		time_measurement.tick("migrate");
		std::mutex time_measurement_mutex;
		auto mig_func = [=, &time_measurement, &time_measurement_mutex](const std::string &hostname, virDomainPtr domain, virConnectPtr destconn, unsigned long flags, Migrate_devices_guard &dev_guard, Migrate_ivshmem_guard &ivshmem_guard, Repin_guard &repin_guard, const std::string &name)
		{
			// Create migrateuri
			std::string migrate_uri = get_migrate_uri(rdma_migration, hostname);
			{
				std::lock_guard<std::mutex> lock(time_measurement_mutex);
				time_measurement.tick("migrate-" + name);
			}
			// Migrate
			auto dest_domain = migrate_domain(domain, destconn, flags, migrate_uri);
			{
				std::lock_guard<std::mutex> lock(time_measurement_mutex);
				time_measurement.tock("migrate-" + name);
			}
			// Set destination domain for guards
			dev_guard.set_destination_domain(dest_domain);
			ivshmem_guard.set_destination_domain(dest_domain);
			repin_guard.set_destination_domain(dest_domain);
		};
		{
			auto mig1 = std::async(std::launch::async, [&](){mig_func(hostname_swap, domain.get(), conn_swap.get(), flags, dev_guard, ivshmem_guard, repin_guard, name);});
			auto mig2 = std::async(std::launch::async, [&](){mig_func(hostname, domain_swap.get(), conn.get(), flags_swap, dev_guard_swap, ivshmem_guard_swap, repin_guard_swap, name_swap);});
		}
		time_measurement.tock("migrate");
	}
}

std::string get_host_ip(const std::string &hostname)
{
	struct addrinfo hints = {};
	hints.ai_family = AF_INET; // ipv4 only
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo *servinfo_tmp_ptr = nullptr;
	int ret;
	if ((ret = getaddrinfo(hostname.c_str(), "http", &hints, &servinfo_tmp_ptr)) != 0)
		throw std::runtime_error("Error getting host ip address: getaddrinfo: " + std::string(gai_strerror(ret)));
	std::shared_ptr<struct addrinfo> servinfo(servinfo_tmp_ptr, [](struct addrinfo *ai){freeaddrinfo(ai);});
	struct sockaddr_in *h = nullptr;
	struct addrinfo *p = nullptr;
	std::vector<std::string> ips;
	for (p = servinfo.get(); p != nullptr; p = p->ai_next) {
		h = reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
		ips.push_back(inet_ntoa(h->sin_addr));
	}
 	FASTLIB_LOG(libvirt_hyp_log, trace) << "Found " << ips.size() << " IPs for hostname " << hostname << ".";
	for (auto &ip : ips)
		FASTLIB_LOG(libvirt_hyp_log, trace) << ip;
	if (ips.size() == 0)
		throw std::runtime_error("Error getting host IP address: No IP addresses found.");
	return ips.front();
}

//
// Libvirt_hypervisor implementation
//

Libvirt_hypervisor::Libvirt_hypervisor(std::vector<std::string> nodes, std::string default_driver, std::string default_transport, unsigned int start_timeout, unsigned int stop_timeout) :
	pci_device_handler(std::make_shared<PCI_device_handler>()),
	nodes(std::move(nodes)),
	default_driver(std::move(default_driver)),
	default_transport(std::move(default_transport)),
	start_timeout(start_timeout),
	stop_timeout(stop_timeout)
{
}

void Libvirt_hypervisor::start(const Start &task, Time_measurement &time_measurement)
{
	(void) time_measurement;
	// Connect to libvirt to libvirt
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	auto conn = connect("", driver);
	// Check if domain already running on a remote host
	if (!task.vm_name.is_valid())
		throw std::runtime_error("vm-name is not valid.");
	auto vm_name = task.vm_name.get();
	check_remote_state(vm_name, nodes, VIR_DOMAIN_SHUTOFF);
	// Get domain
	std::shared_ptr<virDomain> domain;
	if (task.xml.is_valid()) {
		std::string xml = task.xml.get();
		// Define domain from XML (or start paused if transient)
		if (task.transient.get_or(false))
			domain = create_from_xml(conn.get(), xml, true);
		else
			domain = define_from_xml(conn.get(), xml);
	} else {
		if (task.transient.get_or(false))
			throw std::runtime_error("XML description is missing which is required to create a transient domain.");
		// Find existing domain
		domain = find_by_name(conn.get(), *task.vm_name);
		// Get domain info + check if in shutdown state
		check_state(domain.get(), VIR_DOMAIN_SHUTOFF);
	}
	// Set memory
	if (task.memory.is_valid()) {
		// TODO: Add separat max memory option
		set_max_memory(domain.get(), task.memory);
		set_memory(domain.get(), task.memory);
	}
	// Set VCPUs
	if (task.vcpus.is_valid()) {
		// TODO: Add separat max vcpus option
		set_max_vcpus(domain.get(), task.vcpus);
		set_vcpus(domain.get(), task.vcpus);
	}
	// Start domain (or resume if transient)
	if (task.transient.get_or(false))
		resume_domain(domain.get());
	else
		create(domain.get());
	// Attach devices
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Attach " << task.pci_addrs.size() << " devices by PCI address.";
    for (auto &addr : task.pci_addrs) {
		PCI_address pci_addr = PCI_address(0, addr.bus, addr.device, addr.funct);
		auto dev = std::make_shared<Device>(pci_addr);
		pci_device_handler->attach_device(domain.get(), dev);
	}
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Attach " << task.pci_ids.size() << " devices by vendor id";
	for (auto &pci_id : task.pci_ids) {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Attach device with PCI-ID " << pci_id.str();
		pci_device_handler->attach_by_id(domain.get(), pci_id);
	}
	if (task.ivshmem.is_valid()) {
		Ivshmem_device ivshmem_device(task.ivshmem->id, task.ivshmem->size);
		attach_ivshmem_device(domain.get(), ivshmem_device);
	}
	// Wait for domain to boot
	if (task.probe_with_ssh.get_or(true)) {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Wait for domain to boot.";
		const auto hostname = task.probe_hostname.is_valid() ? task.probe_hostname.get() : get_domain_name(domain.get());
		probe_ssh_connection(hostname, std::chrono::seconds(start_timeout));
	}
}

void Libvirt_hypervisor::stop(const Stop &task, Time_measurement &time_measurement)
{
	(void) time_measurement;
	// Connect to libvirt to libvirt
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	auto func = [&](const std::string &vm_name){
		auto conn = connect("", driver);
		// Get domain by name
		std::shared_ptr<virDomain> domain(
			find_by_name(conn.get(), vm_name)
		);
		// Get domain info + check if in running state
		check_state(domain.get(), VIR_DOMAIN_RUNNING);
		// Check if domain is persistent
		auto persistent = (driver == "lxctools") ? true : is_persistent(domain.get());
		// Detach PCI devices
		pci_device_handler->detach(domain.get());
		// Destroy or shutdown domain
		if (task.force.is_valid() && task.force.get()) {
			if (virDomainDestroy(domain.get()) == -1)
				throw std::runtime_error("Error destroying domain.");
		} else {
			if (virDomainShutdown(domain.get()) == -1)
				throw std::runtime_error("Error shutting domain down.");
		}
		// Wait until domain is shut down
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Wait until domain is shut down.";
		try {
			wait_for_state(domain.get(), VIR_DOMAIN_SHUTOFF, std::chrono::seconds(stop_timeout));
		} catch (const std::runtime_error &e) {
			auto libvirt_error = virGetLastError();
			if (!libvirt_error || persistent || (libvirt_error->code != VIR_ERR_NO_DOMAIN))
				throw e;
		}
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain is shut down.";
		// Undefine if requested
		if (task.undefine && *task.undefine) {
			if (virDomainUndefine(domain.get()) == -1)
				throw std::runtime_error("Error undefining domain.");
		}
	};
	if (task.vm_name) {
		func(*task.vm_name);
	} else if (task.regex) {
		auto conn = connect("", driver);
		auto vm_names = get_active_domain_names(conn.get());
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Using regex: " << *task.regex << ".";
		std::regex regex(*task.regex);
		std::vector<std::future<void>> handles;
		for (auto &vm_name : vm_names) {
			FASTLIB_LOG(libvirt_hyp_log, trace) << "Checking vm_name: " << vm_name << ".";
			if (std::regex_match(vm_name, regex)) {
				FASTLIB_LOG(libvirt_hyp_log, trace) << vm_name << " is a match.";
				handles.push_back(std::async(std::launch::async, func, vm_name));
			}
		}
	} else {
		throw std::runtime_error("Error: Either vm-name or regex must be defined in stop task.");
	}
}

void Libvirt_hypervisor::migrate(const Migrate &task, Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm)
{
	const std::string &dest_hostname = task.dest_hostname;
	auto migration_type = task.migration_type.is_valid() ? task.migration_type.get() : "warm";
	bool rdma_migration = task.rdma_migration.is_valid() ? task.rdma_migration.get() : false;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	auto transport = task.transport.is_valid() ? task.transport.get() : default_transport;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Migrate " << task.vm_name << " to " << task.dest_hostname << ".";
	FASTLIB_LOG(libvirt_hyp_log, trace) << "migration-type=" << migration_type;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "rdma-migration=" << rdma_migration;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "driver=" << driver;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "transport=" << transport;
	// Set migration flags
	auto base_flags = get_migrate_flags(migration_type);
	// Swap migration or normal migration
	if (task.swap_with.is_valid()) {
		if (driver != "qemu")
			throw std::runtime_error("Currently swap migration is only supported by the qemu driver.");
		swap_migration(task.vm_name, task.swap_with.get().vm_name, get_hostname(), dest_hostname, base_flags, base_flags, rdma_migration, driver, transport, task, comm, time_measurement);
	} else {
		auto flags = base_flags;
		// Connect to libvirt
		auto conn = connect("", driver);
		// Get domain by name
		auto domain = find_by_name(conn.get(), task.vm_name);
		// Check if domain is in running state
		check_state(domain.get(), VIR_DOMAIN_RUNNING);
		// Suspend pscom (resume in destructor)
		Pscom_handler pscom_handler(task, comm, time_measurement);
		// Guard migration of PCI devices.
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Create guard for device migration.";
		Migrate_ivshmem_guard ivshmem_guard(domain, time_measurement);
		Migrate_devices_guard dev_guard(pci_device_handler, domain, time_measurement);
		// Guard repin of vcpus.
		// In particular, resume after migration since repin is done after migration in suspended state.
		Repin_guard repin_guard(domain, flags, task.vcpu_map, time_measurement);
		// Connect to destination
		auto dest_connection = connect(dest_hostname, driver, transport);
		// Create migrateuri
		// TODO: Fix libvirt lxctools driver so no IP has to be sent via migrate uri.
		std::string migrate_uri = (driver == "lxctools") ?
			get_host_ip(dest_hostname) :
			get_migrate_uri(rdma_migration, dest_hostname);
		// Migrate domain
		time_measurement.tick("migrate");
		auto dest_domain = migrate_domain(domain.get(), dest_connection.get(), flags, migrate_uri);
		time_measurement.tock("migrate");
		// Set destination domain for guards
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Set destination domain for guards.";
		repin_guard.set_destination_domain(dest_domain);
		dev_guard.set_destination_domain(dest_domain);
		ivshmem_guard.set_destination_domain(dest_domain);
	}
}

int get_capacity(const std::string &host, const std::string &driver, const std::string transport = "")
{
	auto conn = connect(host, driver, transport);
	auto cpu_count = get_host_cpu_count(conn.get());
	auto domain_count = get_active_domain_names(conn.get()).size();
	return cpu_count - domain_count;
}

std::tuple<std::deque<std::pair<std::string, int>> &, std::mutex &> get_destinations_capacities()
{
	static std::mutex dest_caps_mutex;
	static std::deque<std::pair<std::string, int>> dest_caps;
	return std::tie(dest_caps, dest_caps_mutex);
}

void init_destinations_capacities(const std::vector<std::string> &destinations, const std::string &driver, const std::string &transport, bool overbooking)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "init dest_caps";
	auto &dest_caps = std::get<0>(get_destinations_capacities());
	dest_caps.clear();
	for (const auto &destination : destinations) {
		dest_caps.emplace_back(destination, get_capacity(destination, driver, transport));
	}
	FASTLIB_LOG(libvirt_hyp_log, trace) << "dest_caps.size() = " << dest_caps.size();
	// If no overbooking allowed -> drop all full hosts
	FASTLIB_LOG(libvirt_hyp_log, trace) << "overbooking:" << overbooking;
	if (!overbooking) {
		dest_caps.erase(std::remove_if(dest_caps.begin(), dest_caps.end(),
				[](const std::pair<std::string, int> &x){return x.second < 1;}),
				dest_caps.end());
	}
	FASTLIB_LOG(libvirt_hyp_log, trace) << "dest_caps.size() = " << dest_caps.size();
}

std::vector<std::shared_ptr<Task>> Libvirt_hypervisor::get_evacuate_tasks(const Task_container &task_cont)
{
	if (task_cont.type(true) != "node evacuated")
		throw std::runtime_error("No evacuate tasks.");
	auto base_task = std::dynamic_pointer_cast<Evacuate>(task_cont.tasks.front());
	auto overbooking = base_task->overbooking.get_or(true);
	auto driver = base_task->driver.get_or(default_driver);
	auto transport = base_task->transport.get_or(default_transport);
	auto conn = connect("", driver);
	auto domain_names = get_active_domain_names(conn.get());
	std::vector<std::shared_ptr<Task>> tasks;
	for (auto &domain_name : domain_names) {
		// TODO: Implement copy constructor for Evacuate task
		auto task = std::make_shared<Evacuate>();
		task->destinations = base_task->destinations;
		task->mode = base_task->mode;
		task->overbooking = base_task->overbooking;
		task->retry_counter = base_task->retry_counter;
		task->migration_type = base_task->migration_type;
		task->rdma_migration = base_task->rdma_migration;
		task->pscom_hook_procs = base_task->pscom_hook_procs;
		task->driver = base_task->driver;
		task->transport = base_task->transport;
		task->vm_name.set(domain_name);
		tasks.push_back(task);
	}
	init_destinations_capacities(base_task->destinations, driver, transport, overbooking);
	return tasks;
}

Migrate conv_evacuate_to_migrate(const std::string &domain_name, const std::string &destination, const Evacuate &task)
{
	// Convert Evacuate task to Migrate task
	Migrate mig_task;
	mig_task.vm_name = domain_name;
	mig_task.dest_hostname = destination;
	mig_task.migration_type = task.migration_type;
	mig_task.rdma_migration = task.rdma_migration;
	mig_task.pscom_hook_procs = task.pscom_hook_procs;
	mig_task.transport = task.transport;
	mig_task.concurrent_execution = task.concurrent_execution;
	mig_task.driver = task.driver;
	return mig_task;
}

std::string get_next_destination(std::deque<std::pair<std::string, int>> &dest_caps, bool overbooking, const std::string &mode, std::mutex &m)
{
	using dest_caps_deque = std::deque<std::pair<std::string, int>>;
	// To few destinations to migrate to -> error
	if (dest_caps.empty())
		throw std::runtime_error("No destination host left to evacuate to.");
	// Lock for thread safety
	std::lock_guard<std::mutex> lock(m);
	// Use first destination in list
	auto destination = dest_caps.front().first;
	// Reduce capacity of destination
	--dest_caps.front().second;
	if (mode == "compact") { // fill host, then go to next
		if (dest_caps.front().second < 1) { // if host full/overbooked
			if (!overbooking) { // no overbooking -> drop host
				dest_caps.pop_front();
			} else { // overbooking -> rotate to next host
				std::rotate(dest_caps.begin(), dest_caps.begin() + 1, dest_caps.end());
			}
		}
	} else if (mode == "scatter") { // rotate through destinations
		if (dest_caps.front().second < 1 && !overbooking) {
				dest_caps.pop_front();
		} else {
			std::rotate(dest_caps.begin(), dest_caps.begin() + 1, dest_caps.end());
		}
	} else if (mode == "auto") { // sort to distribute domains water-filling-like
		std::sort(dest_caps.begin(), dest_caps.end(),
				[](dest_caps_deque::const_reference a, dest_caps_deque::const_reference b)
				{return a.second > b.second;});
	}
	return destination;
}

void Libvirt_hypervisor::evacuate(const Evacuate &task, Time_measurement &time_measurement, std::shared_ptr<fast::Communicator> comm)
{
	auto mode = task.mode.get_or("auto");
	auto overbooking = task.overbooking.get_or(true);
	auto driver = task.driver.get_or(default_driver);
	auto transport = task.transport.get_or(default_transport);
	auto domain_name = task.vm_name.get();
	// Connect to libvirt
	auto conn = connect("", driver);
	// Get cap per destination and mutex for synchronization in pair
	auto dest_caps_tuple = get_destinations_capacities();
	const auto &destination = get_next_destination(std::get<0>(dest_caps_tuple), overbooking, mode, std::get<1>(dest_caps_tuple));
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Evacuate domain " << domain_name << " to " << destination << ".";
	// Convert task
	auto mig_task = conv_evacuate_to_migrate(domain_name, destination, task);
	// Migrate
	migrate(mig_task, time_measurement, comm);
}

void Libvirt_hypervisor::repin(const Repin &task, Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto &vcpu_map = task.vcpu_map;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	// Connect to libvirt
	auto conn = connect("", driver);
	// Get domain by name
	auto domain = find_by_name(conn.get(), task.vm_name);
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Repin domain " << task.vm_name << ".";
	repin_vcpus(domain.get(), vcpu_map);
}

void Libvirt_hypervisor::suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	// Connect to libvirt
	auto conn = connect("", driver);
	// Get domain by name
	auto domain = find_by_name(conn.get(), task.vm_name);
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Suspend domain " << task.vm_name << ".";
	suspend_domain(domain.get());
}

void Libvirt_hypervisor::resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	// Connect to libvirt
	auto conn = connect("", driver);
	// Get domain by name
	auto domain = find_by_name(conn.get(), task.vm_name);
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Resume domain " << task.vm_name << ".";
	resume_domain(domain.get());
}
