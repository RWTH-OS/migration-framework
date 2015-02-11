#include "task_handler.hpp"
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
		Task_handler task_handler("migfra.conf");
		LOG_PRINT(LOG_DEBUG, "task_handler loop started.");
		task_handler.loop();
		LOG_PRINT(LOG_DEBUG, "task_handler loop closed.");
	} catch (const std::exception &e) {
		LOG_STREAM(LOG_ERR, "Exception: " << e.what());
	}
	return EXIT_SUCCESS;
}
