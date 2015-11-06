/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef TASK_HPP
#define TASK_HPP

#include "hypervisor.hpp"
#include "pci_device_handler.hpp"

#include <fast-lib/communication/communicator.hpp>
#include <fast-lib/serialization/serializable.hpp>

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>

/**
 * \brief RAII-style thread counter.
 *
 * Each instance of Thread_counter represents a running thread.
 * The constructor increments and the destructor decrements the counter.
 * A condition variable is used to wait for the counter to become zero.
 * 
 * TODO: Move static member variables to seperate class to provide multiple thread counters.
 * TODO: Proper testing.
 */
class Thread_counter
{
public:
	/**
	 * \brief Constructor increments counter.
	 */
	Thread_counter();

	/**
	 * \brief Destructor decrements counter.
	 */
	~Thread_counter();

	/**
	 * \brief Wait until count is zero using a condition variable.
	 */
	static void wait_for_threads_to_finish();
private:
	static unsigned int count;
	static std::mutex count_mutex;
	static std::condition_variable count_cv;
};

/**
 * \brief Represents the result of a Sub_task.
 *
 * Results are sent back packed in a vector representing all results of a Task.
 */
struct Result : public fast::Serializable
{
	Result() = default;
	Result(std::string vm_name, std::string status, std::string details = "");
	std::string vm_name;
	std::string status;
	std::string details;

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;
};
YAML_CONVERT_IMPL(Result)

/**
 * \brief Contains a vector of results and enables proper YAML conversion.
 *
 * A Result_container is neccesary to convert results to YAML in the right format.
 */
struct Result_container : public fast::Serializable
{
	Result_container() = default;
	Result_container(const std::string &yaml_str);
	Result_container(std::string title, std::vector<Result> results);
	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;

	std::string title;
	std::vector<Result> results;
};
YAML_CONVERT_IMPL(Result_container)

/**
 * \brief An abstract class to provide an interface for a Sub_task.
 */
class Sub_task : public fast::Serializable
{
public:
	Sub_task() = default;
	/**
	 * \brief Constructor for Sub_task.
	 *
	 * \param concurrent_execution Execute Sub_task in dedicated thread.
	 */
	Sub_task(bool concurrent_execution);
	virtual ~Sub_task(){};
	virtual std::future<Result> execute(std::shared_ptr<Hypervisor> hypervisor,
					    std::shared_ptr<fast::Communicator> comm) = 0;

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;
protected:
	bool concurrent_execution;
};
YAML_CONVERT_IMPL(Sub_task)

/**
 * \brief Generic task class containing sub tasks.
 *
 * Contains several Sub_tasks and executes those.
 * Task_handler will call execute method to execute the task.
 */
class Task :
	public fast::Serializable
{
public:
	/**
	 * \brief Generate trivial default constructor.
	 *
	 * Constructs a Task without sub tasks.
	 * The execute method will return immediatly on a such constructed Task.
	 */
	Task() = default;
	/**
	 * \brief Constructor for Task.
	 *
	 * \param sub_tasks The sub tasks to execute.
	 * \param concurrent_execution Create and wait on subtasks to be finished in dedicated thread.
	 */
	Task(std::vector<std::shared_ptr<Sub_task>> sub_tasks, bool concurrent_execution);

	/**
	 * \brief Execute the task.
	 *
	 * Executes all sub_tasks.
	 * Starts a thread if concurrent_execution is true.
	 * \param hypervisor Hypervisor to be used for execution.
	 * \param comm Communicator to be used to send results.
	 */
	void execute(std::shared_ptr<Hypervisor> hypervisor, std::shared_ptr<fast::Communicator> comm);

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;

	class no_task_exception : std::runtime_error
	{
	public:
		no_task_exception(const std::string &str) : std::runtime_error(str)
		{}
	};
private:
	std::vector<std::shared_ptr<Sub_task>> sub_tasks;
	bool concurrent_execution;

	/**
	 * \brief Get readable type of tasks.
	 *
	 * Returned type is the same format as in YAML (task:/result:).
	 * \param enable_result_format Set to true if type should be stored in Result, else Task format is used.
	 */
	std::string type(bool enable_result_format = false) const;
};
YAML_CONVERT_IMPL(Task)

/**
 * \brief Sub_task to start a single virtual machine.
 */
class Start :
	public Sub_task
{
public:
	Start() = default;
	/**
	 * \brief Constructor for Start sub task.
	 *
	 * \param vm_name The name of the virtual machine to start.
	 * \param vcpus The number of virtual cpus to assign to the virtual machine.
	 * \param memory The ram to assign to the virtual machine in MiB.
	 * \param concurrent_execution Execute this Sub_task in dedicated thread.
	 */
	Start(std::string vm_name, unsigned int vcpus, unsigned long memory, std::vector<PCI_id> pci_ids, bool concurrent_execution);

	/**
	 * \brief Execute the Sub_task.
	 *
	 * \return Returns Result of Sub_task.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::future<Result> execute(std::shared_ptr<Hypervisor> hypervisor,
				    std::shared_ptr<fast::Communicator> comm);

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;
private:
	std::string vm_name;
	unsigned int vcpus;
	unsigned long memory;
	std::vector<PCI_id> pci_ids;
};
YAML_CONVERT_IMPL(Start)

/**
 * \brief Sub_task to stop a single virtual machine.
 */
class Stop :
	public Sub_task
{
public:
	Stop() = default;
	/**
	 * \brief Constructor for Stop sub task.
	 *
	 * \param vm_name The name of the virtual machine to stop.
	 * \param concurrent_execution Execute this Sub_task in dedicated thread.
	 */
	Stop(std::string vm_name, bool force, bool concurrent_execution);
	
	/**
	 * \brief Execute the Sub_task.
	 *
	 * \return Returns Result of Sub_task.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::future<Result> execute(std::shared_ptr<Hypervisor> hypervisor,
				    std::shared_ptr<fast::Communicator> comm);

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;

private:
	std::string vm_name;
	bool force;
};
YAML_CONVERT_IMPL(Stop)

/**
 * \brief Sub_task to migrate a virtual machine.
 */
class Migrate : 
	public Sub_task
{
public:
	Migrate() = default;
	/**
	 * \brief Constructor for Migrate sub task.
	 *
	 * \param vm_name The name of the virtual machine to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Option to enable live migration.
	 * \param rdma_migration Option to enable rdma migration.
	 * \param concurrent_execution Execute this Sub_task in dedicated thread.
	 */
	Migrate(std::string vm_name, std::string dest_hostname, bool live_migration, bool rdma_migration, bool concurrent_execution, unsigned int pscom_hook_procs);

	/**
	 * \brief Execute the Sub_task.
	 *
	 * \return Returns Result of Sub_task.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::future<Result> execute(std::shared_ptr<Hypervisor> hypervisor,
				    std::shared_ptr<fast::Communicator> comm);

	YAML::Node emit() const override;
	void load(const YAML::Node &node) override;

private:
	std::string vm_name;
	std::string dest_hostname;
	bool live_migration;
	bool rdma_migration;
	unsigned int pscom_hook_procs;
};
YAML_CONVERT_IMPL(Migrate)

#endif
