#ifndef TASK_HANDLER_HPP
#define TASK_HANDLER_HPP

#include <memory>

#include "communicator.hpp"
#include "hypervisor.hpp"

class Task_handler
{
public:
	Task_handler();
	~Task_handler();
	void loop();
private:
	std::shared_ptr<Communicator> comm;
	std::shared_ptr<Hypervisor> hypervisor;
	bool running;
};

#endif
