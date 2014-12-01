#include "libvirt_hypervisor.hpp"

#include "logging.hpp"

#include <libvirt/libvirt.h>
#include <stdexcept>

void Libvirt_hypervisor::start(const std::string &vm_name, size_t vcpus, size_t memory)
{
	LOG_STREAM(LOG_DEBUG, "vm_name: " << vm_name << ", vcpus: " << vcpus << ", memory " << memory);
	LOG_PRINT(LOG_ERR, "No implementation of start in libvirt hypervisor wrapper!");
	throw std::logic_error("start not implemented.");
}

void Libvirt_hypervisor::stop(const std::string &vm_name)
{
	LOG_STREAM(LOG_DEBUG, "vm_name: " << vm_name);
	LOG_PRINT(LOG_ERR, "No implementation of stop in libvirt hypervisor wrapper!");
	throw std::logic_error("stop not implemented.");
}

void Libvirt_hypervisor::migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration)
{
	LOG_STREAM(LOG_DEBUG, "vm_name: " << vm_name << ", dest_hostname: " << dest_hostname << ", live_migration " << live_migration);
	LOG_PRINT(LOG_ERR, "No implementation of migrate in libvirt hypervisor wrapper!");
	throw std::logic_error("migrate not implemented.");
}
