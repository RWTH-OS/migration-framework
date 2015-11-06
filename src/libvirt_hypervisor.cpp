/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#include "libvirt_hypervisor.hpp"

#include "pci_device_handler.hpp"

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <boost/log/trivial.hpp>
#include <libssh/libsshpp.hpp>

#include <stdexcept>
#include <memory>
#include <thread>
#include <chrono>

// Some deleter to be used with smart pointers.

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

void probe_ssh_connection(virDomainPtr domain)
{
	auto host = virDomainGetName(domain);
	ssh::Session session;
	session.setOption(SSH_OPTIONS_HOST, host);
	bool success = false;
	do {
		try {
			BOOST_LOG_TRIVIAL(trace) << "Try to connect to domain with SSH.";
			session.connect();
			BOOST_LOG_TRIVIAL(trace) << "Domain is ready.";
			success = true;
		} catch (ssh::SshException &e) {
			BOOST_LOG_TRIVIAL(debug) << "Exception while connecting with SSH: " << e.getError();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		session.disconnect();
	} while (!success);
}

//
// Libvirt_hypervisor implementation
//

/// TODO: Get hostname dynamically.
Libvirt_hypervisor::Libvirt_hypervisor() :
	local_host_conn(virConnectOpen("qemu:///system")),
	pci_device_handler(std::make_shared<PCI_device_handler>())
{
	if (!local_host_conn)
		throw std::runtime_error("Failed to connect to qemu on local host.");
}

Libvirt_hypervisor::~Libvirt_hypervisor()
{
	if (virConnectClose(local_host_conn)) {
		BOOST_LOG_TRIVIAL(trace) << "Warning: Some qemu connections have not been closed after destruction of hypervisor wrapper!";
	}
}

void Libvirt_hypervisor::start(const std::string &vm_name, unsigned int vcpus, unsigned long memory, const std::vector<PCI_id> &pci_ids)
{
	// Get domain by name
	BOOST_LOG_TRIVIAL(trace) << "Get domain by name.";
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in shutdown state
	BOOST_LOG_TRIVIAL(trace) << "Get domain info + check if in shutdown state.";
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_SHUTOFF)
		throw std::runtime_error("Wrong domain state: " + std::to_string(domain_info.state));
	// Set memory
	BOOST_LOG_TRIVIAL(trace) << "Set memory.";
	if (virDomainSetMemoryFlags(domain.get(), memory, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_MEM_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum amount of memory to " + std::to_string(memory) + " KiB for domain " + vm_name);
	if (virDomainSetMemoryFlags(domain.get(), memory, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting amount of memory to " + std::to_string(memory) + " KiB for domain " + vm_name);
	// Set VCPUs
	BOOST_LOG_TRIVIAL(trace) << "Set VCPUs.";
	if (virDomainSetVcpusFlags(domain.get(), vcpus, VIR_DOMAIN_AFFECT_CONFIG | VIR_DOMAIN_VCPU_MAXIMUM) == -1)
		throw std::runtime_error("Error setting maximum number of vcpus to " + std::to_string(vcpus) + " for domain " + vm_name);
	if (virDomainSetVcpusFlags(domain.get(), vcpus, VIR_DOMAIN_AFFECT_CONFIG) == -1)
		throw std::runtime_error("Error setting number of vcpus to " + std::to_string(vcpus) + " for domain " + vm_name);
	// Create domain
	BOOST_LOG_TRIVIAL(trace) << "Create domain.";
	if (virDomainCreate(domain.get()) == -1)
		throw std::runtime_error(std::string("Error creating domain: ") + virGetLastErrorMessage());
	// Attach device
	BOOST_LOG_TRIVIAL(trace) << "Attach " << pci_ids.size() << " devices.";
	for (auto &pci_id : pci_ids) {
		BOOST_LOG_TRIVIAL(trace) << "Attach device with PCI-ID " << pci_id.str();
		pci_device_handler->attach(domain.get(), pci_id);
	}
	// Wait for domain to boot
	probe_ssh_connection(domain.get());
}

void Libvirt_hypervisor::stop(const std::string &vm_name, bool force)
{
	// Get domain by name
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in running state
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_RUNNING)
		throw std::runtime_error("Domain not running.");
	// Detach PCI devices
	pci_device_handler->detach(domain.get());
	// Destroy or shutdown domain
	if (force) {
		if (virDomainDestroy(domain.get()) == -1)
			throw std::runtime_error("Error destroying domain.");
	} else {
		if (virDomainShutdown(domain.get()) == -1)
			throw std::runtime_error("Error shutting domain down.");
	}
	// Wait until domain is shut down
	BOOST_LOG_TRIVIAL(trace) << "Wait until domain is shut down.";
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	while (domain_info.state != VIR_DOMAIN_SHUTOFF) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (virDomainGetInfo(domain.get(), &domain_info) == -1)
			throw std::runtime_error("Failed getting domain info.");
	}
	BOOST_LOG_TRIVIAL(trace) << "Domain is shut down.";
}

void Libvirt_hypervisor::migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool rdma_migration)
{
	BOOST_LOG_TRIVIAL(trace) << "Migrate " << vm_name << " to " << dest_hostname << ".";
	BOOST_LOG_TRIVIAL(trace) << std::boolalpha << "live-migration=" << live_migration;
	BOOST_LOG_TRIVIAL(trace) << std::boolalpha << "rdma-migration=" << rdma_migration;
	// Get domain by name
	BOOST_LOG_TRIVIAL(trace) << "Get domain by name.";
	std::unique_ptr<virDomain, Deleter_virDomain> domain(
		virDomainLookupByName(local_host_conn, vm_name.c_str())
	);
	if (!domain)
		throw std::runtime_error("Domain not found.");
	// Get domain info + check if in running state
	BOOST_LOG_TRIVIAL(trace) << "Get domain info and check if in running state.";
	virDomainInfo domain_info;
	if (virDomainGetInfo(domain.get(), &domain_info) == -1)
		throw std::runtime_error("Failed getting domain info.");
	if (domain_info.state != VIR_DOMAIN_RUNNING)
		throw std::runtime_error("Domain not running.");
	// Guard migration of PCI devices.
	BOOST_LOG_TRIVIAL(trace) << "Create guard for device migration.";
	Migrate_devices_guard dev_guard(pci_device_handler, domain.get());
	// Connect to destination
	BOOST_LOG_TRIVIAL(trace) << "Connect to destination.";
	std::unique_ptr<virConnect, Deleter_virConnect> dest_connection(
		virConnectOpen(("qemu+ssh://" + dest_hostname + "/system").c_str())
	);
	if (!dest_connection)
		throw std::runtime_error("Cannot establish connection to " + dest_hostname);
	// Set migration flags
	unsigned long flags = 0;
	flags |= live_migration ? VIR_MIGRATE_LIVE : 0;
	// create migrateuri
	std::string migrate_uri = rdma_migration ? "rdma://" + dest_hostname + "-ib" : "";
	BOOST_LOG_TRIVIAL(trace) << (rdma_migration ? "Use migrate uri: " + migrate_uri + "." : "Use default migrate uri.");
	// Migrate domain
	BOOST_LOG_TRIVIAL(trace) << "Migrate domain.";
	std::unique_ptr<virDomain, Deleter_virDomain> dest_domain(
		virDomainMigrate(domain.get(), dest_connection.get(), flags, 0, rdma_migration ? migrate_uri.c_str() : nullptr, 0)
	);
	if (!dest_domain)
		throw std::runtime_error(std::string("Migration failed: ") + virGetLastErrorMessage());
	// Set destination domain for guards
	BOOST_LOG_TRIVIAL(trace) << "Set destination domain for guards.";
	dev_guard.set_destination_domain(dest_domain.get());
	// Reattach devices on destination.
	BOOST_LOG_TRIVIAL(trace) << "Reattach devices on destination.";
	dev_guard.reattach();
}
