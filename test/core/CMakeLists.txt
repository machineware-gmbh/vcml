 ##############################################################################
 #                                                                            #
 # Copyright (C) 2022 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

macro(core_test test)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} testing)
    target_compile_options(${test} PRIVATE ${MWR_COMPILER_WARN_FLAGS})
    target_include_directories(${test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${test} PROPERTIES CXX_CLANG_TIDY "${VCML_LINTER}")
    add_test(NAME core/${test} COMMAND ${test} ${VCML_TEST_RESOURCES})
    set_tests_properties(core/${test} PROPERTIES ENVIRONMENT "${VCML_ENVVAR}")
    set_tests_properties(core/${test} PROPERTIES TIMEOUT 4)
endmacro()

core_test("hello")
core_test("sysc")
core_test("logging")
core_test("version")
core_test("dmi")
core_test("range")
core_test("exmon")
core_test("property")
core_test("broker")
core_test("hierarchy")
core_test("module")
core_test("component")
core_test("peripheral")
core_test("register")
core_test("processor")
core_test("gpio")
core_test("clk")
core_test("spi")
core_test("sd")
core_test("i2c")
core_test("pci")
core_test("eth")
core_test("can")
core_test("serial")
core_test("adapter")
core_test("virtio")
core_test("display")
core_test("symtab")
core_test("thctl")
core_test("suspender")
core_test("async")
core_test("stubs")
core_test("tracing")
core_test("async_timer")
core_test("memory")
core_test("disk")
core_tesT("model")

if(LUA_FOUND)
    core_test("lua")
endif()

