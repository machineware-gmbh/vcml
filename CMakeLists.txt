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

cmake_minimum_required(VERSION 3.5)
project(vcml VERSION 2020.09.27 LANGUAGES C CXX)

option(VCML_BUILD_TESTS "Build unit tests" OFF)
option(VCML_BUILD_UTILS "Build utility programs" ON)
option(VCML_BUILD_PIC   "Build position independent code" ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
find_package(SystemC "2.3.0" REQUIRED)
find_package(LibELF REQUIRED)
find_package(LibVNC)
find_package(SDL2)

include(GenVersionHeader)

if(VCML_BUILD_PIC)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(inc ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(gen ${CMAKE_CURRENT_BINARY_DIR}/gen)

set(sources
    ${src}/vcml/common/types.cpp
    ${src}/vcml/common/report.cpp
    ${src}/vcml/common/utils.cpp
    ${src}/vcml/common/strings.cpp
    ${src}/vcml/common/version.cpp
    ${src}/vcml/common/aio.cpp
    ${src}/vcml/common/thctl.cpp
    ${src}/vcml/common/bitops.cpp
    ${src}/vcml/common/systemc.cpp
    ${src}/vcml/logging/logger.cpp
    ${src}/vcml/logging/log_file.cpp
    ${src}/vcml/logging/log_stream.cpp
    ${src}/vcml/logging/log_term.cpp
    ${src}/vcml/properties/property_base.cpp
    ${src}/vcml/properties/property_provider.cpp
    ${src}/vcml/properties/property_provider_arg.cpp
    ${src}/vcml/properties/property_provider_env.cpp
    ${src}/vcml/properties/property_provider_file.cpp
    ${src}/vcml/backends/backend.cpp
    ${src}/vcml/backends/backend_null.cpp
    ${src}/vcml/backends/backend_file.cpp
    ${src}/vcml/backends/backend_term.cpp
    ${src}/vcml/backends/backend_stdout.cpp
    ${src}/vcml/backends/backend_tcp.cpp
    ${src}/vcml/backends/backend_tap.cpp
    ${src}/vcml/debugging/symtab.cpp
    ${src}/vcml/debugging/target.cpp
    ${src}/vcml/debugging/suspender.cpp
    ${src}/vcml/debugging/rspserver.cpp
    ${src}/vcml/debugging/gdbserver.cpp
    ${src}/vcml/debugging/vspserver.cpp
    ${src}/vcml/ui/keymap.cpp
    ${src}/vcml/ui/fbmode.cpp
    ${src}/vcml/ui/display.cpp
    ${src}/vcml/elf.cpp
    ${src}/vcml/sbi.cpp
    ${src}/vcml/dmi_cache.cpp
    ${src}/vcml/exmon.cpp
    ${src}/vcml/module.cpp
    ${src}/vcml/component.cpp
    ${src}/vcml/master_socket.cpp
    ${src}/vcml/slave_socket.cpp
    ${src}/vcml/register.cpp
    ${src}/vcml/peripheral.cpp
    ${src}/vcml/stubs.cpp
    ${src}/vcml/processor.cpp
    ${src}/vcml/system.cpp
    ${src}/vcml/setup.cpp
    ${src}/vcml/spi.cpp
    ${src}/vcml/sd.cpp
    ${src}/vcml/models/generic/clock.cpp
    ${src}/vcml/models/generic/reset.cpp
    ${src}/vcml/models/generic/bus.cpp
    ${src}/vcml/models/generic/memory.cpp
    ${src}/vcml/models/generic/gpio.cpp
    ${src}/vcml/models/generic/simdev.cpp
    ${src}/vcml/models/generic/crossbar.cpp
    ${src}/vcml/models/generic/uart8250.cpp
    ${src}/vcml/models/generic/rtc1742.cpp
    ${src}/vcml/models/generic/spibus.cpp
    ${src}/vcml/models/generic/spi2sd.cpp
    ${src}/vcml/models/generic/sdcard.cpp
    ${src}/vcml/models/generic/sdhci.cpp
    ${src}/vcml/models/generic/hwrng.cpp
    ${src}/vcml/models/generic/fbdev.cpp
    ${src}/vcml/models/generic/throttle.cpp
    ${src}/vcml/models/virtio/virtio.cpp
    ${src}/vcml/models/virtio/mmio.cpp
    ${src}/vcml/models/virtio/rng.cpp
    ${src}/vcml/models/virtio/console.cpp
    ${src}/vcml/models/virtio/input.cpp
    ${src}/vcml/models/opencores/ompic.cpp
    ${src}/vcml/models/opencores/ethoc.cpp
    ${src}/vcml/models/opencores/ockbd.cpp
    ${src}/vcml/models/opencores/ocfbc.cpp
    ${src}/vcml/models/opencores/ocspi.cpp
    ${src}/vcml/models/arm/pl011uart.cpp
    ${src}/vcml/models/arm/pl190vic.cpp
    ${src}/vcml/models/arm/sp804timer.cpp
    ${src}/vcml/models/arm/syscon.cpp
    ${src}/vcml/models/arm/gic400.cpp
    ${src}/vcml/models/riscv/clint.cpp
    ${src}/vcml/models/riscv/plic.cpp
    ${src}/vcml/models/riscv/simdev.cpp)

add_library(vcml ${sources})
set_target_properties(vcml PROPERTIES DEBUG_POSTFIX "d")
target_compile_features(vcml PUBLIC cxx_std_11)
target_compile_definitions(vcml PUBLIC $<$<CONFIG:DEBUG>:VCML_DEBUG>)

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
