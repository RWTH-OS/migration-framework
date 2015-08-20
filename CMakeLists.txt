#
# This file is part of migration-framework.
# Copyright (C) 2015 RWTH Aachen University - ACS
#
# This file is licensed under the GNU Lesser General Public License Version 3
# Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
#

# TODO: Write installation of daemon.
# TODO: Create automatic documentation generation target.

cmake_minimum_required(VERSION 2.8)
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
set(USE_BOOST_CACHE ON)
set(BOOST_CACHE_DIR ${PROJECT_BINARY_DIR})
include(add_boost)
include_directories(SYSTEM ${BoostSourceDir})
list(APPEND LIBS "${BoostProgramOptionsLibs}" "${BoostLogLibs}" "${BoostChronoLibs}" "${BoostDateTimeLibs}" "${BoostFilesystemLibs}" "${BoostThreadLibs}" "${BoostSystemLibs}" "${BoostPropertyTreeLibs}" "${BoostRegexLibs}")
list(APPEND LIBS_BENCHMARK "${BoostProgramOptionsLibs}")
message(STATUS "${LIBS}")

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
	${PROJECT_SOURCE_DIR}/src/libvirt_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/dummy_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/task_handler.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
	${PROJECT_SOURCE_DIR}/src/hooks.cpp
)
set(SRC_BENCHMARK ${PROJECT_SOURCE_DIR}/src/benchmark_main.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
	${PROJECT_SOURCE_DIR}/src/hooks.cpp
)

# fast-lib
include(ExternalProject)
ExternalProject_Add(
	fastlib
	GIT_REPOSITORY git@github.com:fast-project/fast-lib.git
	CMAKE_ARGS -DCMAKE_CXX_FLAGS=-fpic -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DUSE_BOOST_CACHE=ON -DBOOST_CACHE_DIR=${BOOST_CACHE_DIR}
)
ExternalProject_Get_Property(fastlib install_dir)
include_directories("${install_dir}/include")
include_directories(SYSTEM "${install_dir}/include/fast-lib/external")
set(FASTLIB_SERIALIZATION_LIB "${install_dir}/lib/libfastlib_serialization.a")
set(FASTLIB_COMMUNICATION_LIB "${install_dir}/lib/libfastlib_communication.a")
list(APPEND LIBS "${FASTLIB_SERIALIZATION_LIB}" "${FASTLIB_COMMUNICATION_LIB}" "-lrt")
list(APPEND LIBS_BENCHMARK "${FASTLIB_SERIALIZATION_LIB}" "${FASTLIB_COMMUNICATION_LIB}" "-lrt")

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

# Add device xml files to build
configure_file(
	"${PROJECT_SOURCE_DIR}/devices/ib_pci_82_00_1.xml"
	"${PROJECT_BINARY_DIR}/devices/ib_pci_82_00_1.xml"
)

### Build and installation targets
# Add executable
add_executable(migfra ${SRC})
add_executable(migfra_benchmark ${SRC_BENCHMARK})

target_link_libraries(migfra ${LIBS})
target_link_libraries(migfra_benchmark ${LIBS_BENCHMARK})
