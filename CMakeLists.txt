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

cmake_minimum_required(VERSION 3.8)
project(vcml VERSION 2022.01.25 LANGUAGES C CXX)

option(VCML_BUILD_TESTS "Build unit tests" OFF)
option(VCML_BUILD_UTILS "Build utility programs" ON)
option(VCML_BUILD_PIC   "Build position independent code" ON)
set(VCML_LINTER "" CACHE STRING "Code linter to use")

if(VCML_BUILD_PIC)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(GenVersionInfo)
include(Sanitizer)

find_package(Git REQUIRED)
find_package(SystemC "2.3.0" REQUIRED)
find_package(LibELF REQUIRED)
find_package(LibVNC)
find_package(LibRFB)
find_package(LibSLIRP)
find_package(SDL2)

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(inc ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(gen ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(${src}/vcml/core/version.h.in
               ${gen}/vcml/core/version.h @ONLY)

add_library(vcml STATIC
    ${src}/vcml/core/types.cpp
    ${src}/vcml/core/report.cpp
    ${src}/vcml/core/utils.cpp
    ${src}/vcml/core/strings.cpp
    ${src}/vcml/core/version.cpp
    ${src}/vcml/core/aio.cpp
    ${src}/vcml/core/thctl.cpp
    ${src}/vcml/core/bitops.cpp
    ${src}/vcml/core/socket.cpp
    ${src}/vcml/core/systemc.cpp
    ${src}/vcml/core/library.cpp
    ${src}/vcml/core/module.cpp
    ${src}/vcml/core/component.cpp
    ${src}/vcml/core/register.cpp
    ${src}/vcml/core/peripheral.cpp
    ${src}/vcml/core/processor.cpp
    ${src}/vcml/core/system.cpp
    ${src}/vcml/core/setup.cpp
    ${src}/vcml/logging/logger.cpp
    ${src}/vcml/logging/publisher.cpp
    ${src}/vcml/logging/log_file.cpp
    ${src}/vcml/logging/log_stream.cpp
    ${src}/vcml/logging/log_term.cpp
    ${src}/vcml/tracing/tracer.cpp
    ${src}/vcml/tracing/tracer_file.cpp
    ${src}/vcml/tracing/tracer_term.cpp
    ${src}/vcml/properties/property_base.cpp
    ${src}/vcml/properties/broker.cpp
    ${src}/vcml/properties/broker_arg.cpp
    ${src}/vcml/properties/broker_env.cpp
    ${src}/vcml/properties/broker_file.cpp
    ${src}/vcml/debugging/symtab.cpp
    ${src}/vcml/debugging/target.cpp
    ${src}/vcml/debugging/elf_reader.cpp
    ${src}/vcml/debugging/loader.cpp
    ${src}/vcml/debugging/subscriber.cpp
    ${src}/vcml/debugging/suspender.cpp
    ${src}/vcml/debugging/rspserver.cpp
    ${src}/vcml/debugging/gdbarch.cpp
    ${src}/vcml/debugging/gdbserver.cpp
    ${src}/vcml/debugging/vspserver.cpp
    ${src}/vcml/ui/video.cpp
    ${src}/vcml/ui/keymap.cpp
    ${src}/vcml/ui/input.cpp
    ${src}/vcml/ui/display.cpp
    ${src}/vcml/ui/console.cpp
    ${src}/vcml/protocols/tlm_sbi.cpp
    ${src}/vcml/protocols/tlm_exmon.cpp
    ${src}/vcml/protocols/tlm_dmi_cache.cpp
    ${src}/vcml/protocols/tlm_stubs.cpp
    ${src}/vcml/protocols/tlm_host.cpp
    ${src}/vcml/protocols/tlm_sockets.cpp
    ${src}/vcml/protocols/tlm_memory.cpp
    ${src}/vcml/protocols/gpio.cpp
    ${src}/vcml/protocols/clk.cpp
    ${src}/vcml/protocols/spi.cpp
    ${src}/vcml/protocols/sd.cpp
    ${src}/vcml/protocols/i2c.cpp
    ${src}/vcml/protocols/pci.cpp
    ${src}/vcml/protocols/eth.cpp
    ${src}/vcml/protocols/can.cpp
    ${src}/vcml/protocols/serial.cpp
    ${src}/vcml/protocols/virtio.cpp
    ${src}/vcml/models/generic/clock.cpp
    ${src}/vcml/models/generic/reset.cpp
    ${src}/vcml/models/generic/bus.cpp
    ${src}/vcml/models/generic/memory.cpp
    ${src}/vcml/models/generic/gpio.cpp
    ${src}/vcml/models/generic/uart8250.cpp
    ${src}/vcml/models/generic/lan9118.cpp
    ${src}/vcml/models/generic/rtc1742.cpp
    ${src}/vcml/models/generic/hwrng.cpp
    ${src}/vcml/models/generic/fbdev.cpp
    ${src}/vcml/models/serial/backend_fd.cpp
    ${src}/vcml/models/serial/backend_file.cpp
    ${src}/vcml/models/serial/backend_tcp.cpp
    ${src}/vcml/models/serial/backend_term.cpp
    ${src}/vcml/models/serial/backend.cpp
    ${src}/vcml/models/serial/terminal.cpp
    ${src}/vcml/models/ethernet/backend.cpp
    ${src}/vcml/models/ethernet/backend_file.cpp
    ${src}/vcml/models/ethernet/backend_tap.cpp
    ${src}/vcml/models/ethernet/bridge.cpp
    ${src}/vcml/models/ethernet/network.cpp
    ${src}/vcml/models/can/backend.cpp
    ${src}/vcml/models/can/backend_file.cpp
    ${src}/vcml/models/can/backend_socket.cpp
    ${src}/vcml/models/can/bridge.cpp
    ${src}/vcml/models/can/bus.cpp
    ${src}/vcml/models/i2c/lm75.cpp
    ${src}/vcml/models/spi/bus.cpp
    ${src}/vcml/models/spi/spi2sd.cpp
    ${src}/vcml/models/sd/sdcard.cpp
    ${src}/vcml/models/sd/sdhci.cpp
    ${src}/vcml/models/pci/device.cpp
    ${src}/vcml/models/pci/host.cpp
    ${src}/vcml/models/virtio/mmio.cpp
    ${src}/vcml/models/virtio/pci.cpp
    ${src}/vcml/models/virtio/rng.cpp
    ${src}/vcml/models/virtio/console.cpp
    ${src}/vcml/models/virtio/input.cpp
    ${src}/vcml/models/meta/loader.cpp
    ${src}/vcml/models/meta/simdev.cpp
    ${src}/vcml/models/meta/throttle.cpp
    ${src}/vcml/models/opencores/ompic.cpp
    ${src}/vcml/models/opencores/ethoc.cpp
    ${src}/vcml/models/opencores/ockbd.cpp
    ${src}/vcml/models/opencores/ocfbc.cpp
    ${src}/vcml/models/opencores/ocspi.cpp
    ${src}/vcml/models/opencores/oci2c.cpp
    ${src}/vcml/models/arm/pl011uart.cpp
    ${src}/vcml/models/arm/pl190vic.cpp
    ${src}/vcml/models/arm/sp804timer.cpp
    ${src}/vcml/models/arm/syscon.cpp
    ${src}/vcml/models/arm/gic400.cpp
    ${src}/vcml/models/riscv/clint.cpp
    ${src}/vcml/models/riscv/plic.cpp
    ${src}/vcml/models/riscv/simdev.cpp)

set_target_properties(vcml PROPERTIES DEBUG_POSTFIX "d")
set_target_properties(vcml PROPERTIES CXX_CLANG_TIDY "${VCML_LINTER}")
set_target_properties(vcml PROPERTIES VERSION "${VCML_VERSION}")
set_target_properties(vcml PROPERTIES SOVERSION "${VCML_VERSION_MAJOR}")

target_compile_options(vcml PRIVATE -Wall -Werror)
target_compile_features(vcml PUBLIC cxx_std_17)
target_compile_definitions(vcml PUBLIC $<$<CONFIG:DEBUG>:VCML_DEBUG>)
target_compile_definitions(vcml PUBLIC SC_INCLUDE_DYNAMIC_PROCESSES)
target_compile_definitions(vcml PUBLIC SC_DISABLE_API_VERSION_CHECK)

target_include_directories(vcml PUBLIC ${inc})
target_include_directories(vcml PUBLIC ${gen})
target_include_directories(vcml PRIVATE ${src})
target_include_directories(vcml SYSTEM PUBLIC ${SYSTEMC_INCLUDE_DIRS})
target_include_directories(vcml SYSTEM PUBLIC ${LIBELF_INCLUDE_DIRS})

target_link_libraries(vcml PUBLIC ${SYSTEMC_LIBRARIES})
target_link_libraries(vcml PUBLIC ${LIBELF_LIBRARIES})
target_link_libraries(vcml PUBLIC -rdynamic)
target_link_libraries(vcml PUBLIC -pthread)
target_link_libraries(vcml PUBLIC -lrt)
target_link_libraries(vcml PUBLIC -ldl)

if(SDL2_FOUND)
    message(STATUS "Building with SDL support")
    target_compile_definitions(vcml PRIVATE HAVE_SDL2)
    target_include_directories(vcml SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
    target_sources(vcml PRIVATE ${src}/vcml/ui/sdl.cpp)
    target_link_libraries(vcml PUBLIC ${SDL2_LIBRARIES})
else()
    message(STATUS "Building without SDL support")
endif()

if(LIBVNC_FOUND)
    message(STATUS "Building with VNC support")
    target_compile_definitions(vcml PRIVATE HAVE_LIBVNC)
    target_include_directories(vcml SYSTEM PRIVATE ${LIBVNC_INCLUDE_DIRS})
    target_sources(vcml PRIVATE ${src}/vcml/ui/vnc.cpp)
    target_link_libraries(vcml PUBLIC ${LIBVNC_LIBRARIES})
else()
    message(STATUS "Building without VNC support")
endif()

if(LIBRFB_FOUND)
    message(STATUS "Building with RFB support")
    target_compile_definitions(vcml PRIVATE HAVE_LIBRFB)
    target_sources(vcml PRIVATE ${src}/vcml/ui/rfb.cpp)
    target_link_libraries(vcml PUBLIC ${LIBRFB_LIBRARIES})
else()
    message(STATUS "Building without RFB support")
endif()

if(LIBSLIRP_FOUND)
    message(STATUS "Building with SLIRP support")
    target_compile_definitions(vcml PRIVATE HAVE_LIBSLIRP)
    target_include_directories(vcml SYSTEM PRIVATE ${LIBSLIRP_INCLUDE_DIRS})
    target_sources(vcml PRIVATE ${src}/vcml/models/ethernet/backend_slirp.cpp)
    target_link_libraries(vcml PUBLIC ${LIBSLIRP_LIBRARIES})
else()
    message(STATUS "Building without SLIRP support")
endif()

install(TARGETS vcml DESTINATION lib)
install(DIRECTORY ${inc}/ DESTINATION include)
install(DIRECTORY ${gen}/ DESTINATION include)

if(VCML_BUILD_UTILS)
    add_subdirectory(utils)
endif()

if(VCML_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
