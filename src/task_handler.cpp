#include "task_handler.hpp"

#include "mqtt_communicator.hpp"
#include "logging.hpp"
#include "libvirt_hypervisor.hpp"
#include "parser.hpp"

#include <mosquittopp.h>

#include <string>
#include <exception>

Task_handler::Task_handler() : 
	comm(new MQTT_communicator("test-id", "topic1", "localhost", 1883, 60)),
	hypervisor(new Libvirt_hypervisor()),
	running(true)
{
}


void Task_handler::loop()
{
	while (running) {
		std::string msg;
		try {
			msg = comm->get_message();
			parser::str_to_task(msg).execute(hypervisor, comm);
		} catch (const YAML::Exception &e) {
			LOG_PRINT(LOG_ERR, "Exception while parsing message.");
			LOG_STREAM(LOG_ERR, e.what());
			LOG_STREAM(LOG_ERR, "msg dump: " << msg);
		} catch (const std::exception &e) {
			LOG_STREAM(LOG_ERR, "Exception: " << e.what());
			LOG_STREAM(LOG_ERR, "msg dump: " << msg);
		}
	}
}
