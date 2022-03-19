 ##############################################################################
 #                                                                            #
 # Copyright 2022 Jan Henrik Weinstock                                        #
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

if(TARGET systemc)
    message(STATUS "Using SystemC at " ${SYSTEMC_HOME})
    return()
endif()

if(DEFINED ENV{TARGET_ARCH})
    set(SYSTEMC_TARGET_ARCH $ENV{TARGET_ARCH})
elseif(DEFINED TARGET_ARCH)
    set(SYSTEMC_TARGET_ARCH ${TARGET_ARCH})
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SYSTEMC_TARGET_ARCH "linux64")
else()
    set(SYSTEMC_TARGET_ARCH "linux")
endif()

if(NOT EXISTS ${SYSTEMC_HOME})
    set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
endif()

if(NOT EXISTS ${SYSTEMC_HOME})
    message(FATAL_ERROR "Could not find SystemC")
endif()

if(EXISTS ${SYSTEMC_HOME}/CMakeLists.txt)
    set(ENABLE_PHASE_CALLBACKS ON CACHE STRING "")
    set(ENABLE_PHASE_CALLBACKS_TRACING ON CACHE STRING "")
    add_subdirectory(${SYSTEMC_HOME} systemc EXCLUDE_FROM_ALL)
    set(SYSTEMC_LIBRARIES systemc)
    set(SYSTEMC_INCLUDE_DIRS $<TARGET_PROPERTY:systemc,INCLUDE_DIRECTORIES>)
else()
    find_path(SYSTEMC_INCLUDE_DIRS NAMES systemc
              HINTS ${SYSTEMC_HOME}/include)

    find_library(SYSTEMC_LIBRARIES NAMES libsystemc.a systemc
                 HINTS ${SYSTEMC_HOME}/lib-${SYSTEMC_TARGET_ARCH}
                       ${SYSTEMC_HOME}/lib)

    if(EXISTS ${SYSTEMC_INCLUDE_DIRS}/tlm/)
        list(APPEND SYSTEMC_INCLUDE_DIRS ${SYSTEMC_INCLUDE_DIRS}/tlm)
    endif()
endif()

set(SYSTEMC_VERSION "")
file(GLOB_RECURSE _sysc_ver_file ${SYSTEMC_HOME}/*/sysc/kernel/sc_ver.h)
if(NOT EXISTS ${_sysc_ver_file})
    message(FATAL_ERROR "Cannot find sc_ver.h")
endif()

file(STRINGS ${_sysc_ver_file} _systemc_ver REGEX
     "^#[\t ]*define[\t ]+SC_VERSION_(MAJOR|MINOR|PATCH)[\t ]+([0-9]+)$")
foreach(VPART MAJOR MINOR PATCH)
    foreach(VLINE ${_systemc_ver})
        if(VLINE MATCHES 
           "^#[\t ]*define[\t ]+SC_VERSION_${VPART}[\t ]+([0-9]+)$")
            set(SYSTEMC_VERSION_${VPART} ${CMAKE_MATCH_1})
            if(SYSTEMC_VERSION)
                string(APPEND SYSTEMC_VERSION .${CMAKE_MATCH_1})
            else()
                set(SYSTEMC_VERSION ${CMAKE_MATCH_1})
            endif()
        endif()
    endforeach()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SystemC
        REQUIRED_VARS SYSTEMC_LIBRARIES SYSTEMC_INCLUDE_DIRS
        VERSION_VAR   SYSTEMC_VERSION)
mark_as_advanced(SYSTEMC_LIBRARIES SYSTEMC_INCLUDE_DIRS)

#message(STATUS "SYSTEMC_FOUND         " ${SYSTEMC_FOUND})
#message(STATUS "SYSTEMC_HOME          " ${SYSTEMC_HOME})
#message(STATUS "SYSTEMC_TARGET_ARCH   " ${SYSTEMC_TARGET_ARCH})
#message(STATUS "SYSTEMC_INCLUDE_DIRS  " ${SYSTEMC_INCLUDE_DIRS})
#message(STATUS "SYSTEMC_LIBRARIES     " ${SYSTEMC_LIBRARIES})
#message(STATUS "SYSTEMC_VERSION_MAJOR " ${SYSTEMC_VERSION_MAJOR})
#message(STATUS "SYSTEMC_VERSION_MINOR " ${SYSTEMC_VERSION_MINOR})
#message(STATUS "SYSTEMC_VERSION_PATCH " ${SYSTEMC_VERSION_PATCH})
#message(STATUS "SYSTEMC_VERSION       " ${SYSTEMC_VERSION})
