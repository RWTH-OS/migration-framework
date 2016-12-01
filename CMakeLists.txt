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

set(ENABLE_LOGGING ON CACHE BOOL "Enable logging.")
if(ENABLE_LOGGING)
	add_definitions(-DFASTLIB_ENABLE_LOGGING)
endif()

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
list(APPEND LIBS "${BoostProgramOptionsLibs}" "${BoostPropertyTreeLibs}" "${BoostRegexLibs}")
list(APPEND LIBS_BENCHMARK "${BoostProgramOptionsLibs}" "${BoostPropertyTreeLibs}" "${BoostRegexLibs}")

# fast-lib
include(ExternalProject)
ExternalProject_Add(
	fastlib
	GIT_REPOSITORY https://github.com/fast-project/fast-lib.git
	CMAKE_COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} cmake
	CMAKE_ARGS -DCMAKE_CXX_FLAGS=-fpic -DBUILD_SHARED_LIBS=OFF -DENABLE_LOGGING=${ENABLE_LOGGING} -DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)
ExternalProject_Get_Property(fastlib install_dir)
include_directories("${install_dir}/include")
include_directories(SYSTEM "${install_dir}/include/fast-lib/external")
set(FASTLIB "${install_dir}/lib/libfastlib.a")
list(APPEND LIBS "${FASTLIB}" "-lrt")
list(APPEND LIBS_BENCHMARK "${FASTLIB}" "-lrt")

# libssh
ExternalProject_Add(
	libssh
	GIT_REPOSITORY git://git.libssh.org/projects/libssh.git
	GIT_TAG libssh-0.7.2
	CMAKE_COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} cmake
	CMAKE_ARGS -DWITH_ZLIB=OFF -DWITH_SFTP=OFF -DWITH_PCAP=OFF -DWITH_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)
ExternalProject_Get_Property(libssh install_dir)
include_directories(SYSTEM "${install_dir}/include")
list(APPEND LIBS "${install_dir}/lib/libssh.so")
#add_definitions(-DLIBSSH_STATIC=1)


# Find libvirt
find_package(LibVirt REQUIRED)
if(LIBVIRT_FOUND)
	include_directories(SYSTEM ${LibVirt_INCLUDE_DIR})
	list(APPEND LIBS "${LibVirt_LIBRARY}")
	list(APPEND LIBS_BENCHMARK "${LibVirt_LIBRARY}")
else()

	message(SEND_ERROR "libvirt is required.")
endif()

### Define source files.
set(SRC ${PROJECT_SOURCE_DIR}/src/main.cpp
	${PROJECT_SOURCE_DIR}/src/libvirt_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/dummy_hypervisor.cpp
	${PROJECT_SOURCE_DIR}/src/task_handler.cpp
	${PROJECT_SOURCE_DIR}/src/task.cpp
	${PROJECT_SOURCE_DIR}/src/pscom_handler.cpp
	${PROJECT_SOURCE_DIR}/src/pci_device_handler.cpp
	${PROJECT_SOURCE_DIR}/src/utility.cpp
)
set(SRC_BENCHMARK ${PROJECT_SOURCE_DIR}/src/benchmark_main.cpp)

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
add_dependencies(migfra fastlib libssh)
add_dependencies(migfra_benchmark fastlib)
target_link_libraries(migfra ${LIBS})
target_link_libraries(migfra_benchmark ${LIBS_BENCHMARK})
