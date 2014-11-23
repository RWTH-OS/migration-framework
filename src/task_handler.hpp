#ifndef TASK_HANDLER_HPP
#define TASK_HANDLER_HPP

#include <memory>

#include "communicator.hpp"

class Task_handler
{
public:
	Task_handler();
	void loop();
private:
	std::unique_ptr<Communicator> comm;
	bool running;
};

#endif
