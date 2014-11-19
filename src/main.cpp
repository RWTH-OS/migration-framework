#include "logging.hpp"
#include "conf.hpp"

#include <cstdlib>

int main(int argc, char *argv[])
{
	LOG_PRINT(LOG_NOTICE, "Hello World");
	return EXIT_SUCCESS;
}
