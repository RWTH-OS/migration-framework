#include "mqtt_communicator.hpp"

#include "logging.hpp"
#include "conf.hpp"

#include <unistd.h>
#include <cstdlib>
#include <string>
#include <exception>

int main(int argc, char *argv[])
{
	getopt(argc, argv, "");
	try {
		LOG_PRINT(LOG_NOTICE, "Initializing migfra...");
		Communicator *comm = new MQTT_communicator("T1", "topic1", "localhost", 1883);
		LOG_PRINT(LOG_NOTICE, "Migfra initialized.");
	
		LOG_PRINT(LOG_NOTICE, "Sending message...");
		comm->send_message("Hello world.");
		LOG_PRINT(LOG_NOTICE, "Message sent.");
	
		LOG_PRINT(LOG_NOTICE, "Quiting migfra...");
		delete comm;
		LOG_PRINT(LOG_NOTICE, "Migfra quit.");
	} catch (const std::exception &e) {
		LOG_PRINT(LOG_ERR, (std::string("Exception: ") + e.what()).c_str());
	}
	return EXIT_SUCCESS;
}
