#include "task.hpp"

#include "parser.hpp"
#include "logging.hpp"

#include <exception>
#include <future>
#include <utility>

Thread_counter::Thread_counter()
{
	std::unique_lock<std::mutex> lock(count_mutex);
	++count;
}

Thread_counter::~Thread_counter()
{
	std::unique_lock<std::mutex> lock(count_mutex);
	if (--count == 0)
		count_cv.notify_one();
}

void Thread_counter::wait_for_threads_to_finish()
{
	LOG_PRINT(LOG_DEBUG, "Waiting for threads to finish...");
	std::unique_lock<std::mutex> lock(count_mutex);
	while (count != 0)
		count_cv.wait(lock);
	LOG_PRINT(LOG_DEBUG, "All threads are finished.");
}

unsigned int Thread_counter::count;
std::mutex Thread_counter::count_mutex;
std::condition_variable Thread_counter::count_cv;


Result::Result(const std::string &title, const std::string &vm_name, const std::string &status, const std::string &details) :
	title(title),
	vm_name(vm_name),
	status(status),
	details(details)
{
}

Sub_task::Sub_task(bool concurrent_execution) :
	concurrent_execution(concurrent_execution)
{
}

Task::Task(std::vector<std::shared_ptr<Sub_task>> sub_tasks, bool concurrent_execution) :
	sub_tasks(std::move(sub_tasks)), concurrent_execution(concurrent_execution)
{
}

void Task::execute(const std::shared_ptr<Hypervisor> &hypervisor, const std::shared_ptr<Communicator> &comm)
{
	if (sub_tasks.empty()) return;
	/// \todo In C++14 unique_ptr for sub_tasks and init capture to move in lambda should be used!
	auto &sub_tasks = this->sub_tasks;
	auto func = [&hypervisor, &comm, sub_tasks] 
	{
		std::vector<std::future<Result>> future_results;
		for (auto &sub_task : sub_tasks) // start subtasks
			future_results.push_back(sub_task->execute(hypervisor));
		std::vector<Result> results;
		for (auto &future_result : future_results) // wait for subtasks to finish
			results.push_back(future_result.get());
		comm->send_message(parser::results_to_str(results));
	};
	concurrent_execution ? std::thread([func]{Thread_counter cnt; func();}).detach() : func();
}

Start::Start(const std::string &vm_name, size_t vcpus, size_t memory, bool concurrent_execution) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name),
	vcpus(vcpus),
	memory(memory)
{
}

std::future<Result> Start::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto &vcpus = this->vcpus;
	auto &memory = this->memory;
	auto func = [&hypervisor, vm_name, vcpus, memory] () 
	{
		try {
			hypervisor->start(vm_name, vcpus, memory);
		} catch (const std::exception &e) {
			return Result("vm started", vm_name, "error", e.what());
		}
		return Result("vm started", vm_name, "success", "");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}

Stop::Stop(const std::string &vm_name, bool concurrent_execution) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name)
{
}

std::future<Result> Stop::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto func = [&hypervisor, vm_name] ()
	{
		try {
			hypervisor->stop(vm_name);
		} catch (const std::exception &e) {
			return Result("vm stopped", vm_name, "error", e.what());
		}
		return Result("vm stopped", vm_name, "success", "");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}

Migrate::Migrate(const std::string &vm_name, const std::string &dest_hostname, bool live_migration, bool concurrent_execution) :
	Sub_task::Sub_task(concurrent_execution),
	vm_name(vm_name),
	dest_hostname(dest_hostname),
	live_migration(live_migration)
{
}

std::future<Result> Migrate::execute(const std::shared_ptr<Hypervisor> &hypervisor)
{
	auto &vm_name = this->vm_name; /// \todo In C++14 init capture should be used!
	auto &dest_hostname = this->dest_hostname;
	auto &live_migration = this->live_migration;
	auto func = [&hypervisor, vm_name, dest_hostname, live_migration] ()
	{
		try {
			hypervisor->migrate(vm_name, dest_hostname, live_migration);
		} catch (const std::exception &e) {
			return Result("migrate done", vm_name, "error", e.what());
		}
		return Result("migrate done", vm_name, "success", "");
	};
	return std::async(concurrent_execution ? std::launch::async : std::launch::deferred, func);
}
