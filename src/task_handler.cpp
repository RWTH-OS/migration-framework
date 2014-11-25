#include "task_handler.hpp"

#include "mqtt_communicator.hpp"
#include "logging.hpp"

#include <mosquittopp.h>

#include <string>
#include <exception>

Task_handler::Task_handler() : 
	comm(new MQTT_communicator("test-id", "topic1", "localhost", 1883)),
	running(true)
{
}


void Task_handler::loop()
{
	while (running) {
		try {
			std::string msg = comm->get_message();
			std::unique_ptr<Task> task = parser.str_to_task(msg);
			comm->send_message(parser.results_to_str(task->execute()));
			if (msg == "quit")
				running = false;
		} catch (const std::exception &e) {
			LOG_STREAM(LOG_ERR, "Exception: " << e.what());
		}
	}
}
