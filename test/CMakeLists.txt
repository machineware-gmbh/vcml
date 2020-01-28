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

enable_testing()

find_package(GTest "1.8.0" REQUIRED)
find_package(GMock "1.8.0" REQUIRED)
include_directories(SYSTEM ${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS})
include_directories(SYSTEM ${SYSTEMC_INCLUDE_DIRS})
set(CMAKE_EXE_LINKER_FLAGS -rdynamic)
add_library(test_main test_main.cpp)

set(tests test_hello test_utils test_logging test_version test_aio test_elf
    test_dmi test_range test_exmon test_module test_component test_peripheral
    test_register test_backend_tcp test_generic_memory test_generic_bus
    test_processor test_property test_spi test_bitops test_riscv_clint
    test_riscv_plic test_sdhci)

if(LIBVNC_FOUND)
    set(tests ${tests} test_vnc)
endif()

foreach(test ${tests})
    add_executable(${test} ${test}.cpp)
    add_dependencies(${test} vcml test_main)
    target_link_libraries(${test} vcml ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES}
                          ${SYSTEMC_LIBRARIES} ${LIBELF_LIBRARIES} test_main
                          -pthread -lrt -lelf)
    if(LIBVNC_FOUND)
        target_link_libraries(${test} ${LIBVNC_LIBRARIES})
    endif()

    add_test(NAME ${test} COMMAND ${test} ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(${test} PROPERTIES TIMEOUT 4)
endforeach()
