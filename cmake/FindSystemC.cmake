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

if(NOT DEFINED SystemC_FIND_QUIET AND NOT DEFINED ENV{SYSTEMC_HOME})
    message(WARNING "SYSTEMC_HOME not defined")
endif()

set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
find_path(SYSTEMC_INCLUDE_DIR NAMES systemc
          HINTS ${SYSTEMC_HOME}/include)

if(DEFINED ENV{TARGET_ARCH})
    set(SYSTEMC_TARGET_ARCH $ENV{TARGET_ARCH})
elseif(DEFINED TARGET_ARCH)
    set(SYSTEMC_TARGET_ARCH ${TARGET_ARCH})
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SYSTEMC_TARGET_ARCH "linux64")
    else()
        set(SYSTEMC_TARGET_ARCH "linux")
    endif()
    if (NOT DEFINED SystemC_FIND_QUIET)
        message(WARNING "TARGET_ARCH not defined, guessing "
                \"${SYSTEMC_TARGET_ARCH}\")
    endif()
endif()

find_library(SYSTEMC_LIBRARY NAMES libsystemc.a systemc
             HINTS ${SYSTEMC_HOME}/lib-${SYSTEMC_TARGET_ARCH})

set(SYSTEMC_INCLUDE_DIRS ${SYSTEMC_INCLUDE_DIR})
set(SYSTEMC_LIBRARIES "${SYSTEMC_LIBRARY}")

set(_sysc_ver_file "${SYSTEMC_INCLUDE_DIR}/sysc/kernel/sc_ver.h")
if(EXISTS ${_sysc_ver_file})
    file(STRINGS ${_sysc_ver_file}  _systemc_ver REGEX
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
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SystemC
    REQUIRED_VARS SYSTEMC_LIBRARY SYSTEMC_INCLUDE_DIR
    VERSION_VAR SYSTEMC_VERSION)

mark_as_advanced(SYSTEMC_INCLUDE_DIR SYSTEMC_LIBRARY)

#message(STATUS "SYSTEMC_FOUND         " ${SYSTEMC_FOUND})
#message(STATUS "SYSTEMC_HOME          " ${SYSTEMC_HOME})
#message(STATUS "SYSTEMC_TARGET_ARCH   " ${SYSTEMC_TARGET_ARCH})
#message(STATUS "SYSTEMC_INCLUDE_DIRS  " ${SYSTEMC_INCLUDE_DIRS})
#message(STATUS "SYSTEMC_LIBRARY       " ${SYSTEMC_LIBRARY})
#message(STATUS "SYSTEMC_LIBRARIES     " ${SYSTEMC_LIBRARIES})
#message(STATUS "SYSTEMC_VERSION_MAJOR " ${SYSTEMC_VERSION_MAJOR})
#message(STATUS "SYSTEMC_VERSION_MINOR " ${SYSTEMC_VERSION_MINOR})
#message(STATUS "SYSTEMC_VERSION_PATCH " ${SYSTEMC_VERSION_PATCH})
#message(STATUS "SYSTEMC_VERSION       " ${SYSTEMC_VERSION})
