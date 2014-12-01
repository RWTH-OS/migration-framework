#ifndef TASK_HPP
#define TASK_HPP

#include "hypervisor.hpp"

#include <string>
#include <vector>
#include <memory>

/**
 * \brief Represents the result of the execute method of a Task.
 */
struct Result
{
	Result(const std::string &title, const std::string &vm_name, const std::string &status);
	std::string title;
	std::string vm_name;
	std::string status;
};

/**
 * \brief Abstract base class for tasks.
 *
 * Every task has to inherit from this class.
 * Task_handler will call execute() to execute the task.
 * TODO: Create Result class to be returned by operator() of tasks.
 */
class Task
{
public:
	/**
	 * \brief Virtual destructor for proper destruction.
	 */
	virtual ~Task(){};

	/**
	 * \brief Execute the task.
	 *
	 * Pure virtual so every derived task has to implement this method.
	 * \return Return type is a vector, because in packed mode multiple tasks return results.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	virtual std::vector<Result> execute(const std::unique_ptr<Hypervisor> &hypervisor) = 0;
};

/**
 * \brief Task to start a virtual machine.
 */
class Start : 
	public Task
{
public:
	/**
	 * \brief Constructor for Start task.
	 *
	 * \param vm_name The name of the virtual machine to start.
	 * \param vcpus The number of virtual cpus to assign to the virtual machine.
	 * \param memory The ram to assign to the virtual machine in MiB.
	 */
	Start(const std::string &vm_name, size_t vcpus, size_t memory);

	/**
	 * \brief Execute the task.
	 *
	 * \return Return type is a vector, because in packed mode multiple tasks return results.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::vector<Result> execute(const std::unique_ptr<Hypervisor> &hypervisor);

private:
	std::string vm_name;
	size_t vcpus;
	size_t memory;
};

/**
 * \brief Task to start virtual machines in packed mode.
 *
 * Executes multiple Start tasks.
 */
class Start_packed :
	public Task
{
public:
	/**
	 * \brief Constructor for Start_packed task.
	 *
	 * \param start_tasks A vector of start tasks to execute.
	 */
	Start_packed(const std::vector<Start> &start_tasks);

	/**
	 * \brief Execute the task.
	 *
	 * \return Return type is a vector, because in packed mode multiple tasks return results.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::vector<Result> execute(const std::unique_ptr<Hypervisor> &hypervisor);

private:
	std::vector<Start> start_tasks;
};

/**
 * \brief Task to stop a virtual machine.
 */
class Stop : 
	public Task
{
public:
	/**
	 * \brief Constructor for Stop task.
	 *
	 * \param vm_name The name of the virtual machine to stop.
	 */
	Stop(const std::string &vm_name);
	
	/**
	 * \brief Execute the task.
	 *
	 * \return Return type is a vector, because in packed mode multiple tasks return results.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::vector<Result> execute(const std::unique_ptr<Hypervisor> &hypervisor);

private:
	std::string vm_name;
};

/**
 * \brief Task to stop virtual machines in packed mode.
 *
 * Executes multiple Stop tasks.
 */
class Stop_packed : 
	public Task
{
public:
	/**
	 * \brief Constructor for Stop_packed task.
	 *
	 * \param stop_tasks A vector of stop tasks to execute.
	 */
	Stop_packed(const std::vector<Stop> &stop_tasks);
	
	/**
	 * \brief Execute the task.
	 *
	 * \return Return type is a vector, because in packed mode multiple tasks return results.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::vector<Result> execute(const std::unique_ptr<Hypervisor> &hypervisor);

private:
	std::vector<Stop> stop_tasks;
};

/**
 * \brief Task to migrate a virtual machine.
 */
class Migrate : 
	public Task
{
public:
	/**
	 * \brief Constructor for Migrate task.
	 *
	 * \param vm_name The name of the virtual machine to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Option to enable live migration.
	 */
	Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration);

	/**
	 * \brief Execute the task.
	 *
	 * \return Return type is a vector, because in packed mode multiple tasks return results.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::vector<Result> execute(const std::unique_ptr<Hypervisor> &hypervisor);

private:
	std::string vm_name;
	std::string dest_hostname;
	bool live_migration;
};

#endif
