 ##############################################################################
 #                                                                            #
 # Copyright (C) 2022 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

if (NOT TARGET gtest OR NOT TARGET gmock)
    option(INSTALL_GTEST OFF)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(googletest EXCLUDE_FROM_ALL)
endif()

if(CMAKE_BUILD_TYPE MATCHES "ASAN")
    string(APPEND VCML_ENVVAR "ASAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/sanitizer/asan.suppress:detect_leaks=1:fast_unwind_on_malloc=0;")
    string(APPEND VCML_ENVVAR "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/sanitizer/lsan.suppress;")
elseif(CMAKE_BUILD_TYPE MATCHES "TSAN")
    string(APPEND VCML_ENVVAR "TSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/sanitizer/tsan.suppress detect_deadlocks=1 second_deadlock_stack=1 history_size=7;")
elseif(CMAKE_BUILD_TYPE MATCHES "UBSAN")
    string(APPEND VCML_ENVVAR "UBSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/sanitizer/ubsan.suppress:print_stacktrace=1;")
endif()

set(VCML_TEST_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

add_library(testing testing.cpp)
target_include_directories(testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(testing vcml gtest gmock)
target_compile_options(testing PUBLIC ${MWR_COMPILER_WARN_FLAGS})

add_subdirectory(core)
add_subdirectory(models)
