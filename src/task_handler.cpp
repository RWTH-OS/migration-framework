#include "task_handler.hpp"

#include "mqtt_communicator.hpp"
#include "logging.hpp"

#include <mosquittopp.h>

#include <string>

Task_handler::Task_handler() : 
	comm(new MQTT_communicator("test-id", "topic1", "localhost", 1883)),
	running(true)
{
}

Task_handler::~Task_handler()
{
	delete comm;
}

void Task_handler::loop()
{
	while (running) {
		LOG_PRINT(LOG_NOTICE, "Waiting for message.");
		std::string msg = comm->get_message();
		LOG_PRINT(LOG_NOTICE, ("Message received: " + msg).c_str());
		if (msg == "quit")
			running = false;
	}
}
