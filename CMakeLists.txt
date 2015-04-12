# TODO: Write installation of daemon.
# TODO: Create automatic documentation generation target.

cmake_minimum_required(VERSION 2.6)
project(migration-framework)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Define version number
set(MIGFRA_VERSION_MAJOR "0")
set(MIGFRA_VERSION_MINOR "1")
set(MIGFRA_VERSION_PATCH "0")
set(MIGFRA_VERSION "${MIGFRA_VERSION_MAJOR}.${MIGFRA_VERSION_MINOR}.${MIGFRA_VERSION_PATCH}")

### Define logging method
# Choose one else logging system is determined automatically.
#set(LOG_USE_NONE 1)
#set(LOG_USE_STDOUT 1)
#set(LOG_USE_SYSLOG 1)
#set(LOG_USE_SYSTEMD 1)

#### Set compiler flags
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" CXX11_SUPPORTED)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" CXX0X_SUPPORTED)
if(CXX11_SUPPORTED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(CXX0X_SUPPORTED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

### Find libraries
# Find logging system
# TODO: Try to find syslog.
if(NOT LOG_USE_NONE OR NOT LOG_USE_STDOUT OR NOT LOG_USE_SYSLOG)
	find_package(SystemdJournal)
	if(LIBSYSTEMD_JOURNAL_FOUND)
		include_directories(${LIBSYSTEMD_JOURNAL_INCLUDE_DIR})
		list(APPEND LIBS "${LIBSYSTEMD_JOURNAL_LIBRARY}")
		list(APPEND LIBS_BENCHMARK "${LIBSYSTEMD_JOURNAL_LIBRARY}")
		list(APPEND LIBS "systemd")
		list(APPEND LIBS_BENCHMARK "systemd")
		set(LOG_USE_SYSTEMD 1)
	elseif(LOG_USE_SYSTEMD)
		message(FATAL_ERROR "systemd journal is required.")
	elseif(NOT LOG_USE_NONE)
		set(LOG_USE_STDOUT 1)
	endif()
endif()

# Find yaml-cpp
find_package(YamlCpp REQUIRED)
if(YAMLCPP_FOUND)
	include_directories(${YAMLCPP_INCLUDE_DIR})
	list(APPEND LIBS "${YAMLCPP_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${YAMLCPP_LIBRARY}")
else()
	message(FATAL_ERROR "yaml-cpp is required.")
endif()

# Find boost
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	list(APPEND LIBS "${Boost_LIBRARIES}")
	list(APPEND LIBS_BENCHMARK "${Boost_LIBRARIES}")
else()
	message(FATAL_ERROR "Boost::program_options is required.")
endif()

# Find mosquittopp
find_package(Mosquittopp REQUIRED)
if(MOSQUITTOPP_FOUND)
	include_directories(${MOSQUITTOPP_INCLUDE_DIR})
	list(APPEND LIBS "${MOSQUITTOPP_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${MOSQUITTOPP_LIBRARY}")
else()
	message(FATAL_ERROR "mosquittopp is required.")
endif()

# Find mosquitto
find_package(Mosquitto REQUIRED)
if(MOSQUITTO_FOUND)
	include_directories(${MOSQUITTO_INCLUDE_DIR})
	list(APPEND LIBS "${MOSQUITTO_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${MOSQUITTO_LIBRARY}")
else()
	message(FATAL_ERROR "mosquitto is required.")
endif()

# Find libvirt
#TODO: Implementation of find_package module for libvirt
list(APPEND LIBS "virt")

### Define source files.
set(SRC ${PROJECT_SOURCE_DIR}/src/main.cpp
	${PROJECT_SOURCE_DIR}/src/mqtt_communicator.cpp
	${PROJECT_SOURCE_DIR}/src/parser.cpp
	${PROJECT_SOURCE_DIR}/src/libvirt_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/task_handler.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
	${PROJECT_SOURCE_DIR}/src/serializable.cpp
)
set(SRC_BENCHMARK ${PROJECT_SOURCE_DIR}/src/benchmark_main.cpp
	${PROJECT_SOURCE_DIR}/src/mqtt_communicator.cpp
	${PROJECT_SOURCE_DIR}/src/parser.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
	${PROJECT_SOURCE_DIR}/src/serializable.cpp
)

### Add config files
# Doxygen documentation generation
configure_file(
	"${PROJECT_SOURCE_DIR}/Doxyfile.in"
	"${PROJECT_BINARY_DIR}/Doxyfile"
	@ONLY
)

# Add conf.hpp to pass CMake settings to src code.
configure_file(
	"${PROJECT_SOURCE_DIR}/src/conf.hpp.in"
	"${PROJECT_BINARY_DIR}/conf.hpp"
)
include_directories("${PROJECT_BINARY_DIR}")

# Add migfra.conf as runtime configuration file.
configure_file(
	"${PROJECT_SOURCE_DIR}/src/migfra.conf.in"
	"${PROJECT_BINARY_DIR}/migfra.conf"
	@ONLY
)

### Build and installation targets
# Add executable
add_executable(migfra ${SRC})
add_executable(migfra_benchmark ${SRC_BENCHMARK})

target_link_libraries(migfra ${LIBS})
target_link_libraries(migfra_benchmark ${LIBS_BENCHMARK})
