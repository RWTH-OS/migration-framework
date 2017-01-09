/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "libvirt_hypervisor.hpp"

#include "pci_device_handler.hpp"
#include "utility.hpp"
#include "ivshmem_handler.hpp"

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <fast-lib/log.hpp>
#include <libssh/libsshpp.hpp>

#include <stdexcept>
#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <mutex>

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
void probe_ssh_connection(virDomainPtr domain)
{
	auto host = virDomainGetName(domain);
	ssh::Session session;
	session.setOption(SSH_OPTIONS_HOST, host);
	bool success = false;
	do {
		try {
			FASTLIB_LOG(libvirt_hyp_log, trace) << "Try to connect to domain with SSH.";
			session.connect();
			FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain is ready.";
			success = true;
		} catch (ssh::SshException &e) {
			FASTLIB_LOG(libvirt_hyp_log, debug) << "Exception while connecting with SSH: " << e.getError();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		session.disconnect();
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
	std::string uri = driver + plus_transport + "://" + host + "/system";
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Connect to " + uri;
	std::shared_ptr<virConnect> conn(
			virConnectOpen(uri.c_str()),
			Deleter_virConnect()
	);
	if (!conn)
		throw std::runtime_error("Failed to connect to libvirt with uri: " + uri);
	return conn;
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
 * \param conn The connection to search the domain on.
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
void wait_for_state(virDomainPtr domain, virDomainState expected_state)
{
	while (get_domain_state(domain) != expected_state) {
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

void suspend_impl(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Suspend domain.";
	if (virDomainSuspend(domain) == -1)
		throw std::runtime_error(std::string("Error suspending domain: ") + virGetLastErrorMessage());
}

void resume_impl(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Resume domain.";
	if (virDomainResume(domain) == -1)
		throw std::runtime_error(std::string("Error resuming domain: ") + virGetLastErrorMessage());
}

void destroy(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Destroy domain.";
	if (virDomainDestroy(domain) == -1)
		throw std::runtime_error(std::string("Error destroying domain: ") + virGetLastErrorMessage());
}

void delete_snapshot(virDomainSnapshotPtr snapshot)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Delete snapshot.";
	if (virDomainSnapshotDelete(snapshot, 0) == -1)
		throw std::runtime_error(std::string("Error deleting snapshot: ") + virGetLastErrorMessage());
}

void revert_to_snapshot(virDomainSnapshotPtr snapshot)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Revert to snapshot.";
	if (virDomainRevertToSnapshot(snapshot, VIR_DOMAIN_SNAPSHOT_REVERT_RUNNING) == -1)
		throw std::runtime_error(std::string("Error reverting snapshot: ") + virGetLastErrorMessage());
}

std::shared_ptr<virDomainSnapshot> create_snapshot(virDomainPtr domain)
{
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Create snapshot";
	std::shared_ptr<virDomainSnapshot> snapshot(
			virDomainSnapshotCreateXML(domain, 
"<domainsnapshot><description>Snapshot for migration</description>\
	<memory snapshot='internal'/>\
</domainsnapshot>"
			, 0),
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
		std::shared_ptr<virDomain> dest_domain(
			virDomainMigrate(domain, dest_conn, flags, 0, migrate_uri != "" ? migrate_uri.c_str() : nullptr, 0),
			Deleter_virDomain()
		);
		if (!dest_domain)
			throw std::runtime_error(std::string("Migration failed: ") + virGetLastErrorMessage());
		return dest_domain;
}

void sort_domains_by_size(std::shared_ptr<virDomain> &domain1, std::string &name1, std::shared_ptr<virConnect> &conn1, std::string &hostname1, std::shared_ptr<virDomain> &domain2, std::string &name2, std::shared_ptr<virConnect> &conn2, std::string &hostname2)
{
	Memory_stats mem_stats1(domain1.get());
	Memory_stats mem_stats2(domain2.get());
	auto domain1_size = mem_stats1.actual_balloon;
	auto domain2_size = mem_stats2.actual_balloon;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain1 size: " << domain1_size;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain2 size: " << domain2_size;
	if (domain1_size > domain2_size) {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Swap domain1 and domain2.";
		std::swap(domain1, domain2);
		std::swap(name1, name2);
		std::swap(conn1, conn2);
		std::swap(hostname1, hostname2);
	}
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

void repin_impl(virDomainPtr domain, const std::vector<std::vector<unsigned int>> &vcpu_map)
{
	// Get number of CPUs on node
	auto maplen = get_cpumaplen(get_connect_of_domain(domain));
	// Create cpumap and pin for each vcpu
	for (unsigned int vcpu = 0; vcpu != vcpu_map.size(); ++vcpu) {
		pin_vcpu_to_cpus(domain, vcpu, vcpu_map[vcpu], maplen);
	}
}

//
// Libvirt_hypervisor implementation
//

Libvirt_hypervisor::Libvirt_hypervisor(std::vector<std::string> nodes, std::string default_driver, std::string default_transport) :
	pci_device_handler(std::make_shared<PCI_device_handler>()),
	nodes(std::move(nodes)),
	default_driver(std::move(default_driver)),
	default_transport(std::move(default_transport))
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
		if (task.ivshmem.is_valid()) {
			auto &ivshmem = task.ivshmem.get();
			std::string path = ivshmem.path.is_valid() ? ivshmem.path.get() : "/tmp/" + ivshmem.id;
			xml = add_ivshmem_dev(xml, ivshmem.id, ivshmem.size, path);
		}
		// Define domain from XML (or start paused if transient)
		if (task.transient.is_valid() && task.transient.get())
			domain = create_from_xml(conn.get(), xml, true);
		else
			domain = define_from_xml(conn.get(), xml);
	} else {
		if (task.transient.is_valid() && task.transient.get())
			throw std::runtime_error("XML description is missing which is required to create a transient domain.");
		// Find existing domain
		domain = find_by_name(conn.get(), task.vm_name);
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
	if (task.transient.is_valid() && task.transient.get())
		resume_impl(domain.get());
	else
		create(domain.get());
	// Attach devices
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Attach " << task.pci_ids.size() << " devices.";
	for (auto &pci_id : task.pci_ids) {
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Attach device with PCI-ID " << pci_id.str();
		pci_device_handler->attach(domain.get(), pci_id);
	}
	// Wait for domain to boot
	probe_ssh_connection(domain.get());
}

void Libvirt_hypervisor::stop(const Stop &task, Time_measurement &time_measurement)
{
	(void) time_measurement;
	// Connect to libvirt to libvirt
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	auto conn = connect("", driver);
	// Get domain by name
	std::shared_ptr<virDomain> domain(
		find_by_name(conn.get(), task.vm_name)
	);
	// Get domain info + check if in running state
	check_state(domain.get(), VIR_DOMAIN_RUNNING);
	// Check if domain is persistent
	auto persistent = is_persistent(domain.get());
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
		wait_for_state(domain.get(), VIR_DOMAIN_SHUTOFF);
	} catch (const std::runtime_error &e) {
		auto libvirt_error = virGetLastError();
		if (!libvirt_error || persistent || (libvirt_error->code != VIR_ERR_NO_DOMAIN))
			throw e;
	}
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Domain is shut down.";
	// Undefine if requested
	if (task.undefine.is_valid() && task.undefine.get()) {
		if (virDomainUndefine(domain.get()) == -1)
			throw std::runtime_error("Error undefining domain.");
	}
}

void Libvirt_hypervisor::migrate(const Migrate &task, Time_measurement &time_measurement)
{
	const std::string &dest_hostname = task.dest_hostname;
	auto migration_type = task.migration_type.is_valid() ? task.migration_type.get() : "warm";
	bool rdma_migration = task.rdma_migration.is_valid() ? task.rdma_migration.get() : false;;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	auto transport = task.transport.is_valid() ? task.transport.get() : default_transport;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "Migrate " << task.vm_name << " to " << task.dest_hostname << ".";
	FASTLIB_LOG(libvirt_hyp_log, trace) << "migration-type=" << migration_type;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "rdma-migration=" << rdma_migration;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "driver=" << driver;
	FASTLIB_LOG(libvirt_hyp_log, trace) << "transport=" << transport;
	// Set migration flags
	auto flags = get_migrate_flags(migration_type);
	// Swap migration or normal migration
	if (task.swap_with.is_valid()) {
		// Get domains
		// domain1 is snapshot-migrated, domain2 is migrated as defined by migration-type
		auto name1 = task.vm_name;
		auto name2 = task.swap_with.get();
		auto hostname1 = get_hostname();
		auto hostname2 = dest_hostname;
		auto conn1 = connect(hostname1, driver, transport);
		auto conn2 = connect(hostname2, driver, transport);
		auto domain1 = find_by_name(conn1.get(), name1);
		auto domain2 = find_by_name(conn2.get(), name2);
		// Check if domains are in running state
		check_state(domain1.get(), VIR_DOMAIN_RUNNING);
		check_state(domain2.get(), VIR_DOMAIN_RUNNING);
		// Compare size and swap if necessary
		// Guard migration of PCI devices.
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Create guards for device migration.";
		Migrate_devices_guard dev_guard1(pci_device_handler, domain1, time_measurement, name1);
		Migrate_devices_guard dev_guard2(pci_device_handler, domain2, time_measurement, name2);
		if (check_snapshot_required(domain1.get(), conn1.get(), domain2.get(), conn2.get())) {
			// TODO: RAII handler for snapshot for better error recovery
			sort_domains_by_size(domain1, name1, conn1, hostname1, domain2, name2, conn2, hostname2);
			// Suspend vm1
			time_measurement.tick("downtime-" + name1);
			time_measurement.tick("suspend-" + name1);
			suspend_impl(domain1.get());
			// Take snapshot of vm1.
			auto snapshot = create_snapshot(domain1.get());
			// destroy vm1
			destroy(domain1.get());
			time_measurement.tock("suspend-" + name1);
			// Create migrateuri for vm2
			std::string migrate_uri = get_migrate_uri(rdma_migration, hostname1);
			// Migrate vm2
			time_measurement.tick("migrate-" + name2);
			auto dest_domain2 = migrate_domain(domain2.get(), conn1.get(), flags, migrate_uri);
			time_measurement.tock("migrate-" + name2);
			// Set destination domain for guard of vm2
			dev_guard2.set_destination_domain(dest_domain2);
			// Get snapshotted domain on dest
			time_measurement.tick("resume-" + name1);
			auto dest_domain1 = find_by_name(conn2.get(), name1);
			// Redefine snapshot on remote
			auto dest_snapshot = redefine_snapshot(dest_domain1.get(), snapshot.get());
			// Remove snapshot from src
			delete_snapshot(snapshot.get());
			// Revert to snapshot
			revert_to_snapshot(dest_snapshot.get());
			time_measurement.tock("resume-" + name1);
			time_measurement.tock("downtime-" + name1);
			// Remove snapshot from destination
			delete_snapshot(dest_snapshot.get());
			// Set destination domain for guard
			dev_guard1.set_destination_domain(dest_domain1);
		} else {
			time_measurement.tick("migrate");
			std::mutex time_measurement_mutex;
			auto mig_func = [rdma_migration, flags, &time_measurement, &time_measurement_mutex](const std::string &hostname, virDomainPtr domain, virConnectPtr destconn, Migrate_devices_guard &dev_guard, const std::string &name)
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
				// Set destination domain for guard
				dev_guard.set_destination_domain(dest_domain);
			};
			{
				auto mig1 = std::async(std::launch::async, [&](){mig_func(hostname2, domain1.get(), conn2.get(), dev_guard1, name1);});
				auto mig2 = std::async(std::launch::async, [&](){mig_func(hostname1, domain2.get(), conn1.get(), dev_guard2, name2);});
			}
			time_measurement.tock("migrate");
		}
	} else {
		// Connect to libvirt
		auto conn = connect("", driver);
		// Get domain by name
		auto domain = find_by_name(conn.get(), task.vm_name);
		// Check if domain is in running state
		check_state(domain.get(), VIR_DOMAIN_RUNNING);
		// Guard migration of PCI devices.
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Create guard for device migration.";
		Migrate_devices_guard dev_guard(pci_device_handler, domain, time_measurement);
		// Connect to destination
		auto dest_connection = connect(dest_hostname, driver, transport);
		// Create migrateuri
		std::string migrate_uri = get_migrate_uri(rdma_migration, dest_hostname);
		// Migrate domain
		time_measurement.tick("migrate");
		auto dest_domain = migrate_domain(domain.get(), dest_connection.get(), flags, migrate_uri);
		time_measurement.tock("migrate");
		// Repin
		if (task.vcpu_map.is_valid()) {
			time_measurement.tick("repin");
			repin_impl(dest_domain.get(), task.vcpu_map.get());
			time_measurement.tock("repin");
		}
		// Set destination domain for guards
		FASTLIB_LOG(libvirt_hyp_log, trace) << "Set destination domain for guards.";
		dev_guard.set_destination_domain(dest_domain);
	}
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
	repin_impl(domain.get(), vcpu_map);
}

void Libvirt_hypervisor::suspend(const fast::msg::migfra::Suspend &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	// Connect to libvirt
	auto conn = connect("", driver);
	// Get domain by name
	auto domain = find_by_name(conn.get(), task.vm_name);
	suspend_impl(domain.get());
}

void Libvirt_hypervisor::resume(const fast::msg::migfra::Resume &task, fast::msg::migfra::Time_measurement &time_measurement)
{
	(void) time_measurement;
	auto driver = task.driver.is_valid() ? task.driver.get() : default_driver;
	// Connect to libvirt
	auto conn = connect("", driver);
	// Get domain by name
	auto domain = find_by_name(conn.get(), task.vm_name);
	resume_impl(domain.get());
}


