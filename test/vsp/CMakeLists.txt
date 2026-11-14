 ##############################################################################
 #                                                                            #
 # Copyright (C) 2026 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This work is licensed under the terms described in the LICENSE file found  #
 # in the root directory of this source tree.                                 #
 #                                                                            #
 ##############################################################################

macro(vsp_test test)
    add_executable(vsp_${test} ${test}.cpp)
    target_link_libraries(vsp_${test} testing)
    target_compile_options(vsp_${test} PRIVATE ${MWR_COMPILER_WARN_FLAGS})
    target_include_directories(vsp_${test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(vsp_${test} PROPERTIES CXX_CLANG_TIDY "${VCML_LINTER}")
    add_test(NAME vsp/${test} COMMAND vsp_${test} ${VCML_TEST_RESOURCES})
    set_tests_properties(vsp/${test} PROPERTIES ENVIRONMENT "${VCML_ENVVAR}")
    set_tests_properties(vsp/${test} PROPERTIES TIMEOUT 60)
endmacro()

vsp_test("events")
vsp_test("server")
vsp_test("disconnect")
vsp_test("quit")
vsp_test("reconnect")
vsp_test("resume")
vsp_test("linger")
vsp_test("memory")
vsp_test("attr")
