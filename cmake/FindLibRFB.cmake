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

if(NOT TARGET librfb AND EXISTS $ENV{LIBRFB_HOME}/CMakeLists.txt)
    add_subdirectory($ENV{LIBRFB_HOME} librfb EXCLUDE_FROM_ALL)
endif()

if(TARGET librfb)
    set(LIBRFB_FOUND TRUE)
    set(LIBRFB_LIBRARIES librfb)
    get_target_property(LIBRFB_INCLUDE_DIRS librfb INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(LIBRFB_VERSION librfb VERSION)
    if(NOT LIBRFB_VERSION)
        set(LIBRFB_VERSION "0.0.0")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBRFB
    REQUIRED_VARS LIBRFB_LIBRARIES LIBRFB_INCLUDE_DIRS
    VERSION_VAR   LIBRFB_VERSION)
mark_as_advanced(LIBRFB_LIBRARIES LIBRFB_INCLUDE_DIRS)

message(DEBUG "LIBRFB_FOUND        " ${LIBRFB_FOUND})
message(DEBUG "LIBRFB_INCLUDE_DIRS " ${LIBRFB_INCLUDE_DIRS})
message(DEBUG "LIBRFB_LIBRARIES    " ${LIBRFB_LIBRARIES})
message(DEBUG "LIBRFB_VERSION      " ${LIBRFB_VERSION})
