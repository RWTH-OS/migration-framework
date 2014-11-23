#ifndef TASK_HANDLER_HPP
#define TASK_HANDLER_HPP

class Communicator;

class Task_handler
{
public:
	Task_handler();
	~Task_handler();
	void loop();
private:
	Communicator *comm;
	bool running;
};

#endif
