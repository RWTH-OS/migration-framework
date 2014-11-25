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
		Task_handler task_handler;
		LOG_PRINT(LOG_NOTICE, "task_handler loop started.");
		task_handler.loop();
		LOG_PRINT(LOG_NOTICE, "task_handler loop closed.");
	} catch (const std::exception &e) {
		LOG_STREAM(LOG_ERR, "Exception: " << e.what());
	}
	return EXIT_SUCCESS;
}
