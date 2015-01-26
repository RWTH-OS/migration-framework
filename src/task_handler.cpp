#include "task_handler.hpp"

#include "mqtt_communicator.hpp"
#include "logging.hpp"
#include "libvirt_hypervisor.hpp"
#include "parser.hpp"
#include "task.hpp"

#include <mosquittopp.h>

#include <string>
#include <exception>
#include <fstream>
#include <sstream>

Task_handler::Task_handler() : 
	hypervisor(std::make_shared<Libvirt_hypervisor>()),
	running(true)
{
	std::ifstream file_stream("migfra.conf");
	std::stringstream string_stream;
	string_stream << file_stream.rdbuf();
	comm = parser::str_to_communicator(string_stream.str());
}

Task_handler::~Task_handler()
{
	Thread_counter::wait_for_threads_to_finish();
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
