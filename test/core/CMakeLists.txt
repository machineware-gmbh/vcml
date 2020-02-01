 ##############################################################################
 #                                                                            #
 # Copyright 2020 Jan Henrik Weinstock                                        #
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

macro(core_test test)
    add_executable(${test} ${test}.cpp)

    target_include_directories(${test} SYSTEM PUBLIC ${SYSTEMC_INCLUDE_DIRS})
    target_link_libraries(${test} vcml gtest gmock)
    target_link_libraries(${test} ${SYSTEMC_LIBRARIES} ${LIBELF_LIBRARIES})
    target_link_libraries(${test} main -pthread -lrt -lelf -rdynamic)

    if(LIBVNC_FOUND)
        target_link_libraries(${test} ${LIBVNC_LIBRARIES})
    endif()

    add_test(NAME core/${test} COMMAND ${test} ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(core/${test} PROPERTIES TIMEOUT 4)
endmacro()

core_test("hello")
core_test("utils")
core_test("bitops")
core_test("logging")
core_test("version")
core_test("backend_tcp")
core_test("aio")
core_test("elf")
core_test("dmi")
core_test("range")
core_test("exmon")
core_test("property")
core_test("module")
core_test("component")
core_test("peripheral")
core_test("register")
core_test("processor")
core_test("spi")

if(LIBVNC_FOUND)
    core_test("vnc")
endif()
