# TODO: Find yaml-cpp library.
# TODO: Find libmosquittopp.
# TODO: Write installation of daemon.

cmake_minimum_required(VERSION 2.6)
project(migration-framework)

# Define version number
set(MIGFRA_VERSION_MAJOR "0")
set(MIGFRA_VERSION_MINOR "0")
set(MIGFRA_VERSION_PATCH "1")
set(MIGFRA_VERSION "${MIGFRA_VERSION_MAJOR}.${MIGFRA_VERSION_MINOR}.${MIGFRA_VERSION_PATCH}")

# Define logging method (TODO: find installed logging system)
# Choose only one:
#set(LOG_USE_NONE 1)
#set(LOG_USE_STDOUT 1)
#set(LOG_USE_SYSLOG 1)
set(LOG_USE_SYSTEMD 1)

# Set compiler flags
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" CXX11_SUPPORTED)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" CXX0X_SUPPORTED)
if(CXX11_SUPPORTED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(CXX0X_SUPPORTED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# Define source files.
set(SRC ${PROJECT_SOURCE_DIR}/src/main.cpp
	${PROJECT_SOURCE_DIR}/src/mqtt_communicator.cpp
	${PROJECT_SOURCE_DIR}/src/parser.cpp
	${PROJECT_SOURCE_DIR}/src/libvirt_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/task_handler.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
)

# Add conf.hpp to pass CMake settings to src code.
configure_file(
	"${PROJECT_SOURCE_DIR}/src/conf.hpp.in"
	"${PROJECT_BINARY_DIR}/conf.hpp"
)
include_directories("${PROJECT_BINARY_DIR}")

# Add executable
add_executable(migfra ${SRC})

if(${LOG_USE_SYSTEMD})
	target_link_libraries(migfra systemd)
	#elseif(${LOG_USE_SYSLOG})
endif()
target_link_libraries(migfra mosquittopp mosquitto)
