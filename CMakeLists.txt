 ##############################################################################
 #                                                                            #
 # Copyright 2018 Jan Henrik Weinstock                                        #
 #                                                                            #
 # Licensed under the Apache License, Version 2.0 (the "License");            #
 # you may not use this file except in compliance with the License.           #
 # You may obtain a copy of the License at                                    #
 #                                                                            #
 #     http://www.apache.org/licenses/LICENSE-2.0                             #
 #                                                                            #
 # Unless required by applicable law or agreed to in writing, software        #
 # distributed under the License is distributed on an "AS IS" BASIS,          #
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
 # See the License for the specific language governing permissions and        #
 # limitations under the License.                                             #
 #                                                                            #
 ##############################################################################

project(vcml)
cmake_minimum_required(VERSION 3.5)

option(BUILD_TESTS "Build unit tests, requires GTest and GMock >= 1.8.0" OFF)
option(BUILD_UTILS "Builds utility programs" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(SystemC "2.3.0" REQUIRED)
find_package(LibElf REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

set(src "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(inc "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(sources
    "${src}/vcml/common/report.cpp"
    "${src}/vcml/common/utils.cpp"
    "${src}/vcml/common/aio.cpp"
    "${src}/vcml/common/thctl.cpp"
    "${src}/vcml/logging/logger.cpp"
    "${src}/vcml/logging/log_file.cpp"
    "${src}/vcml/logging/log_stream.cpp"
    "${src}/vcml/logging/log_term.cpp"
    "${src}/vcml/properties/property_base.cpp"
    "${src}/vcml/properties/property_provider.cpp"
    "${src}/vcml/properties/property_provider_arg.cpp"
    "${src}/vcml/properties/property_provider_env.cpp"
    "${src}/vcml/properties/property_provider_file.cpp"
    "${src}/vcml/backends/backend.cpp"
    "${src}/vcml/backends/backend_null.cpp"
    "${src}/vcml/backends/backend_file.cpp"
    "${src}/vcml/backends/backend_term.cpp"
    "${src}/vcml/backends/backend_stdout.cpp"
    "${src}/vcml/backends/backend_tcp.cpp"
    "${src}/vcml/backends/backend_tap.cpp"
    "${src}/vcml/debugging/rspserver.cpp"
    "${src}/vcml/debugging/gdbserver.cpp"
    "${src}/vcml/elf.cpp"
    "${src}/vcml/dmi_cache.cpp"
    "${src}/vcml/exmon.cpp"
    "${src}/vcml/component.cpp"
    "${src}/vcml/master_socket.cpp"
    "${src}/vcml/slave_socket.cpp"
    "${src}/vcml/register.cpp"
    "${src}/vcml/peripheral.cpp"
    "${src}/vcml/ports.cpp"
    "${src}/vcml/stubs.cpp"
    "${src}/vcml/processor.cpp"
    "${src}/vcml/models/generic/bus.cpp"
    "${src}/vcml/models/generic/memory.cpp"
    "${src}/vcml/models/generic/crossbar.cpp"
    "${src}/vcml/models/generic/uart8250.cpp"
    "${src}/vcml/models/opencores/ompic.cpp"
    "${src}/vcml/models/opencores/ethoc.cpp"
    "${src}/vcml/models/arm/pl011uart.cpp"
    "${src}/vcml/models/arm/pl190vic.cpp"
    "${src}/vcml/models/arm/sp804timer.cpp")

add_library(vcml ${sources})
target_include_directories(vcml PUBLIC ${inc})
target_include_directories(vcml SYSTEM PUBLIC ${SYSTEMC_INCLUDE_DIRS})

install(TARGETS vcml DESTINATION lib)
install(DIRECTORY ${inc}/ DESTINATION include)

if(BUILD_UTILS)
    add_subdirectory(utils)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
