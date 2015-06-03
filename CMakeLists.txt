#
# This file is part of migration-framework.
# Copyright (C) 2015 RWTH Aachen University - ACS
#
# This file is licensed under the GNU Lesser General Public License Version 3
# Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
#

# TODO: Write installation of daemon.
# TODO: Create automatic documentation generation target.

cmake_minimum_required(VERSION 2.6)
project(migration-framework)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

### Find libraries
# Find yaml-cpp
find_package(YamlCpp REQUIRED)
if(YAMLCPP_FOUND)
	include_directories(SYSTEM ${YAMLCPP_INCLUDE_DIR})
	list(APPEND LIBS "${YAMLCPP_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${YAMLCPP_LIBRARY}")
else()
	message(SEND_ERROR "yaml-cpp is required.")
endif()

# Find boost
find_package(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
	include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
	list(APPEND LIBS "${Boost_LIBRARIES}")
	list(APPEND LIBS_BENCHMARK "${Boost_LIBRARIES}")
else()
	message(SEND_ERROR "Boost::program_options is required.")
endif()

# Find mosquittopp
find_package(Mosquittopp REQUIRED)
if(MOSQUITTOPP_FOUND)
	include_directories(SYSTEM ${MOSQUITTOPP_INCLUDE_DIR})
	list(APPEND LIBS "${MOSQUITTOPP_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${MOSQUITTOPP_LIBRARY}")
else()
	message(SEND_ERROR "mosquittopp is required.")
endif()

# Find mosquitto
find_package(Mosquitto REQUIRED)
if(MOSQUITTO_FOUND)
	include_directories(SYSTEM ${MOSQUITTO_INCLUDE_DIR})
	list(APPEND LIBS "${MOSQUITTO_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${MOSQUITTO_LIBRARY}")
else()
	message(SEND_ERROR "mosquitto is required.")
endif()

# Find libvirt
find_package(LibVirt REQUIRED)
if(LIBVIRT_FOUND)
	include_directories(SYSTEM ${LibVirt_INCLUDE_DIR})
	list(APPEND LIBS "${LibVirt_LIBRARY}")
else()

	message(SEND_ERROR "libvirt is required.")
endif()

### Define source files.
set(SRC ${PROJECT_SOURCE_DIR}/src/main.cpp
	${PROJECT_SOURCE_DIR}/src/mqtt_communicator.cpp
	${PROJECT_SOURCE_DIR}/src/parser.cpp
	${PROJECT_SOURCE_DIR}/src/libvirt_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/task_handler.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
)
set(SRC_BENCHMARK ${PROJECT_SOURCE_DIR}/src/benchmark_main.cpp
	${PROJECT_SOURCE_DIR}/src/mqtt_communicator.cpp
	${PROJECT_SOURCE_DIR}/src/parser.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
)

# fast-lib
include_directories(${PROJECT_SOURCE_DIR})
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/fast-lib/CMakeLists.txt")
	message(SEND_ERROR "fast-lib submodule is empty.\nForgot \"git submodule init && git submodule update\"?")
else()
	add_subdirectory(${PROJECT_SOURCE_DIR}/fast-lib)
	list(APPEND LIBS fastlib_serialization)
	list(APPEND LIBS_BENCHMARK fastlib_serialization)
endif()

### Add config files
# Doxygen documentation generation
configure_file(
	"${PROJECT_SOURCE_DIR}/Doxyfile.in"
	"${PROJECT_BINARY_DIR}/Doxyfile"
	@ONLY
)

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
