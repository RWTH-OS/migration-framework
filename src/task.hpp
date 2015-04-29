#ifndef TASK_HPP
#define TASK_HPP

#include "hypervisor.hpp"

#include "communicator.hpp"

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
struct Result
{
	Result(const std::string &title, const std::string &vm_name, const std::string &status, const std::string &details);
	std::string title;
	std::string vm_name;
	std::string status;
	std::string details;
};

/**
 * \brief An abstract class to provide an interface for a Sub_task.
 */
class Sub_task
{
public:
	/**
	 * \brief Constructor for Sub_task.
	 *
	 * \param concurrent_execution Execute Sub_task in dedicated thread.
	 */
	Sub_task(bool concurrent_execution);
	virtual ~Sub_task(){};
	virtual std::future<Result> execute(const std::shared_ptr<Hypervisor> &hypervisor) = 0;
protected:
	bool concurrent_execution;
};

/**
 * \brief Generic task class containing sub tasks.
 *
 * Contains several Sub_tasks and executes those.
 * Task_handler will call execute method to execute the task.
 */
class Task
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
	void execute(const std::shared_ptr<Hypervisor> &hypervisor, const std::shared_ptr<Communicator> &comm);
private:
	std::vector<std::shared_ptr<Sub_task>> sub_tasks;
	bool concurrent_execution;
};

/**
 * \brief Sub_task to start a single virtual machine.
 */
class Start :
	public Sub_task
{
public:
	/**
	 * \brief Constructor for Start sub task.
	 *
	 * \param vm_name The name of the virtual machine to start.
	 * \param vcpus The number of virtual cpus to assign to the virtual machine.
	 * \param memory The ram to assign to the virtual machine in MiB.
	 * \param concurrent_execution Execute this Sub_task in dedicated thread.
	 */
	Start(const std::string &vm_name, size_t vcpus, size_t memory, bool concurrent_execution);

	/**
	 * \brief Execute the Sub_task.
	 *
	 * \return Returns Result of Sub_task.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::future<Result> execute(const std::shared_ptr<Hypervisor> &hypervisor);

private:
	std::string vm_name;
	size_t vcpus;
	size_t memory;
};

/**
 * \brief Sub_task to stop a single virtual machine.
 */
class Stop :
	public Sub_task
{
public:
	/**
	 * \brief Constructor for Stop sub task.
	 *
	 * \param vm_name The name of the virtual machine to stop.
	 * \param concurrent_execution Execute this Sub_task in dedicated thread.
	 */
	Stop(const std::string &vm_name, bool concurrent_execution);
	
	/**
	 * \brief Execute the Sub_task.
	 *
	 * \return Returns Result of Sub_task.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::future<Result> execute(const std::shared_ptr<Hypervisor> &hypervisor);

private:
	std::string vm_name;
};

/**
 * \brief Sub_task to migrate a virtual machine.
 */
class Migrate : 
	public Sub_task
{
public:
	/**
	 * \brief Constructor for Migrate sub task.
	 *
	 * \param vm_name The name of the virtual machine to migrate.
	 * \param dest_hostname The name of the host to migrate to.
	 * \param live_migration Option to enable live migration.
	 * \param concurrent_execution Execute this Sub_task in dedicated thread.
	 */
	Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool concurrent_execution, unsigned int pscom_hook_procs);

	/**
	 * \brief Execute the Sub_task.
	 *
	 * \return Returns Result of Sub_task.
	 * \param hypervisor Hypervisor to be used for execution.
	 */
	std::future<Result> execute(const std::shared_ptr<Hypervisor> &hypervisor);

private:
	std::string vm_name;
	std::string dest_hostname;
	bool live_migration;
	unsigned int pscom_hook_procs;
};

#endif
