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

macro(model_test test)
    add_executable(${test} ${test}.cpp)

    target_include_directories(${test} SYSTEM PUBLIC ${SYSTEMC_INCLUDE_DIRS})
    target_link_libraries(${test} vcml gtest gmock)
    target_link_libraries(${test} ${SYSTEMC_LIBRARIES} ${LIBELF_LIBRARIES})
    target_link_libraries(${test} main -pthread -lrt -lelf -rdynamic)

    if(LIBVNC_FOUND)
        target_link_libraries(${test} ${LIBVNC_LIBRARIES})
    endif()

    add_test(NAME models/${test} COMMAND ${test} ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(models/${test} PROPERTIES TIMEOUT 4)
endmacro()

model_test("generic_bus")
model_test("generic_memory")
model_test("sdhci")
model_test("riscv_clint")
model_test("riscv_plic")
