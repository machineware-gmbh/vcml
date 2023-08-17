 ##############################################################################
 #                                                                            #
 # Copyright (C) 2023 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

cmake_minimum_required(VERSION 3.11)
project(vcml VERSION 2023.07.31 LANGUAGES C CXX)

option(VCML_USE_SDL2 "Use SDL2 for rendering" ON)
option(VCML_USE_VNC "Use VNC for rendering" ON)
option(VCML_USE_RFB "Use RFB for rendering" OFF)
option(VCML_USE_SLIRP "Use SLIRP for networking" ON)
option(VCML_USE_LUA "Use LUA for scripting" ON)
option(VCML_BUILD_TESTS "Build unit tests" OFF)
option(VCML_BUILD_UTILS "Build utility programs" ON)
option(VCML_BUILD_PIC   "Build position independent code" ON)
option(VCML_COVERAGE "Enable generation of code coverage data" OFF)
set(VCML_LINTER "" CACHE STRING "Code linter to use")

include(cmake/common.cmake)
find_github_repo(mwr "machineware-gmbh/mwr")
find_package(Git REQUIRED)
find_package(SystemC "2.3.0" REQUIRED)

if(VCML_USE_VNC)
    find_package(LibVNC)
endif()
if(VCML_USE_RFB)
    find_package(LibRFB)
endif()
if(VCML_USE_SLIRP)
    find_package(LibSLIRP)
endif()
if(VCML_USE_SDL2)
    find_package(SDL2)
endif()
if(VCML_USE_LUA)
    find_package(Lua "5.3")
endif()

set(src ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(inc ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(gen ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(${src}/vcml/core/version.h.in
               ${gen}/vcml/core/version.h @ONLY)

add_library(vcml STATIC
    ${src}/vcml/core/types.cpp
    ${src}/vcml/core/thctl.cpp
    ${src}/vcml/core/systemc.cpp
    ${src}/vcml/core/module.cpp
    ${src}/vcml/core/component.cpp
    ${src}/vcml/core/register.cpp
    ${src}/vcml/core/peripheral.cpp
    ${src}/vcml/core/processor.cpp
    ${src}/vcml/core/system.cpp
    ${src}/vcml/core/setup.cpp
    ${src}/vcml/core/model.cpp
    ${src}/vcml/logging/logger.cpp
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
    ${src}/vcml/models/generic/gate.cpp
    ${src}/vcml/models/generic/gpio.cpp
    ${src}/vcml/models/generic/hwrng.cpp
    ${src}/vcml/models/generic/fbdev.cpp
    ${src}/vcml/models/serial/backend_fd.cpp
    ${src}/vcml/models/serial/backend_file.cpp
    ${src}/vcml/models/serial/backend_tcp.cpp
    ${src}/vcml/models/serial/backend_term.cpp
    ${src}/vcml/models/serial/backend_tui.cpp
    ${src}/vcml/models/serial/backend.cpp
    ${src}/vcml/models/serial/terminal.cpp
    ${src}/vcml/models/serial/uart8250.cpp
    ${src}/vcml/models/serial/pl011.cpp
    ${src}/vcml/models/serial/nrf51.cpp
    ${src}/vcml/models/timers/rtc1742.cpp
    ${src}/vcml/models/timers/sp804.cpp
    ${src}/vcml/models/timers/nrf51.cpp
    ${src}/vcml/models/timers/pl031.cpp
    ${src}/vcml/models/block/backend_file.cpp
    ${src}/vcml/models/block/backend_ram.cpp
    ${src}/vcml/models/block/backend.cpp
    ${src}/vcml/models/block/disk.cpp
    ${src}/vcml/models/ethernet/backend.cpp
    ${src}/vcml/models/ethernet/backend_file.cpp
    ${src}/vcml/models/ethernet/backend_tap.cpp
    ${src}/vcml/models/ethernet/bridge.cpp
    ${src}/vcml/models/ethernet/network.cpp
    ${src}/vcml/models/ethernet/lan9118.cpp
    ${src}/vcml/models/ethernet/ethoc.cpp
    ${src}/vcml/models/can/backend.cpp
    ${src}/vcml/models/can/backend_file.cpp
    ${src}/vcml/models/can/backend_socket.cpp
    ${src}/vcml/models/can/bridge.cpp
    ${src}/vcml/models/can/bus.cpp
    ${src}/vcml/models/i2c/lm75.cpp
    ${src}/vcml/models/i2c/oci2c.cpp
    ${src}/vcml/models/spi/bus.cpp
    ${src}/vcml/models/spi/spi2sd.cpp
    ${src}/vcml/models/spi/max31855.cpp
    ${src}/vcml/models/spi/flash.cpp
    ${src}/vcml/models/spi/ocspi.cpp
    ${src}/vcml/models/sd/card.cpp
    ${src}/vcml/models/sd/sdhci.cpp
    ${src}/vcml/models/pci/device.cpp
    ${src}/vcml/models/pci/host.cpp
    ${src}/vcml/models/virtio/mmio.cpp
    ${src}/vcml/models/virtio/pci.cpp
    ${src}/vcml/models/virtio/rng.cpp
    ${src}/vcml/models/virtio/blk.cpp
    ${src}/vcml/models/virtio/console.cpp
    ${src}/vcml/models/virtio/input.cpp
    ${src}/vcml/models/meta/loader.cpp
    ${src}/vcml/models/meta/simdev.cpp
    ${src}/vcml/models/meta/throttle.cpp
    ${src}/vcml/models/opencores/ompic.cpp
    ${src}/vcml/models/opencores/ockbd.cpp
    ${src}/vcml/models/opencores/ocfbc.cpp
    ${src}/vcml/models/arm/pl190vic.cpp
    ${src}/vcml/models/arm/syscon.cpp
    ${src}/vcml/models/arm/gic400.cpp
    ${src}/vcml/models/riscv/clint.cpp
    ${src}/vcml/models/riscv/plic.cpp
    ${src}/vcml/models/riscv/simdev.cpp)

set_target_properties(vcml PROPERTIES DEBUG_POSTFIX "d")
set_target_properties(vcml PROPERTIES CXX_CLANG_TIDY "${VCML_LINTER}")
set_target_properties(vcml PROPERTIES VERSION "${VCML_VERSION}")
set_target_properties(vcml PROPERTIES SOVERSION "${VCML_VERSION_MAJOR}")

target_compile_options(vcml PRIVATE ${MWR_COMPILER_WARN_FLAGS})
target_compile_features(vcml PUBLIC cxx_std_17)
target_compile_definitions(vcml PUBLIC $<$<CONFIG:DEBUG>:VCML_DEBUG>)
target_compile_definitions(vcml PUBLIC SC_INCLUDE_DYNAMIC_PROCESSES)
target_compile_definitions(vcml PUBLIC SC_DISABLE_API_VERSION_CHECK)

target_include_directories(vcml PUBLIC ${inc})
target_include_directories(vcml PUBLIC ${gen})
target_include_directories(vcml PRIVATE ${src})
target_include_directories(vcml SYSTEM PUBLIC ${SYSTEMC_INCLUDE_DIRS})

target_link_libraries(vcml PUBLIC mwr)
target_link_libraries(vcml PUBLIC ${SYSTEMC_LIBRARIES})

if(SDL2_FOUND)
    message(STATUS "Building with SDL2 support")
    target_compile_definitions(vcml PRIVATE HAVE_SDL2)
    target_include_directories(vcml SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
    target_sources(vcml PRIVATE ${src}/vcml/ui/sdl.cpp)
    target_link_libraries(vcml PUBLIC ${SDL2_LIBRARIES})
else()
    message(STATUS "Building without SDL2 support")
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
    target_include_directories(vcml SYSTEM PRIVATE ${LIBRFB_INCLUDE_DIRS})
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

if(LUA_FOUND)
    message(STATUS "Building with LUA support")
    target_compile_definitions(vcml PRIVATE HAVE_LUA)
    target_include_directories(vcml SYSTEM PRIVATE ${LUA_INCLUDE_DIR})
    target_sources(vcml PRIVATE ${src}/vcml/properties/broker_lua.cpp)
    target_link_libraries(vcml PUBLIC ${LUA_LIBRARIES})
else()
    message(STATUS "Building without LUA support")
    target_sources(vcml PRIVATE ${src}/vcml/properties/broker_nolua.cpp)
endif()

if(VCML_COVERAGE)
    target_compile_options(vcml PUBLIC --coverage)
    target_link_libraries(vcml PUBLIC -lgcov)
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
