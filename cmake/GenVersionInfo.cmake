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

# Including this file will extract version information from the cmake project
# and (if present) surrounding git repository and import the following cmake
# variables into your scope, intended for use with configure_file:
#
#     <PROJECT_NAME>_VERSION_MAJOR
#     <PROJECT_NAME>_VERSION_MINOR
#     <PROJECT_NAME>_VERSION_PATCH
#     <PROJECT_NAME>_GIT_REV
#     <PROJECT_NAME>_GIT_REV_SHORT
#     <PROJECT_NAME>_VERSION
#     <PROJECT_NAME>_VERSION_INT
#     <PROJECT_NAME>_VERSION_STRING

find_package(Git REQUIRED)

string(TOUPPER ${PROJECT_NAME} _pfx)
string(REPLACE "-" "_" _pfx ${_pfx})

set(${_pfx}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(${_pfx}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(${_pfx}_VERSION_PATCH ${PROJECT_VERSION_PATCH})

if (${${_pfx}_VERSION_MINOR} LESS 10)
    set(${_pfx}_VERSION_MINOR 0${${_pfx}_VERSION_MINOR})
endif()

if (${${_pfx}_VERSION_PATCH} LESS 10)
    set(${_pfx}_VERSION_PATCH 0${${_pfx}_VERSION_PATCH})
endif()

execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE ${_pfx}_GIT_REV
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE ${_pfx}_GIT_REV_SHORT
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${GIT_EXECUTABLE} diff --quiet
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE ${_pfx}_GIT_DIRTY
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT ${_pfx}_GIT_REV)
    set(${_pfx}_GIT_REV "nogit")
    set(${_pfx}_GIT_REV_SHORT "nogit")
elseif (${_pfx}_GIT_DIRTY)
    string(APPEND ${_pfx}_GIT_REV "-dirty")
    string(APPEND ${_pfx}_GIT_REV_SHORT "-dirty")
endif()

string(CONCAT ${_pfx}_VERSION ${${_pfx}_VERSION_MAJOR} "."
                              ${${_pfx}_VERSION_MINOR} "."
                              ${${_pfx}_VERSION_PATCH})

string(CONCAT ${_pfx}_VERSION_INT ${${_pfx}_VERSION_MAJOR}
                                  ${${_pfx}_VERSION_MINOR}
                                  ${${_pfx}_VERSION_PATCH})

string(CONCAT ${_pfx}_VERSION_STRING ${PROJECT_NAME} "-"
                                     ${${_pfx}_VERSION} "-"
                                     ${${_pfx}_GIT_REV_SHORT})

#message(STATUS "${_pfx}_VERSION_MAJOR  = ${${_pfx}_VERSION_MAJOR}")
#message(STATUS "${_pfx}_VERSION_MINOR  = ${${_pfx}_VERSION_MINOR}")
#message(STATUS "${_pfx}_VERSION_PATCH  = ${${_pfx}_VERSION_PATCH}")
#message(STATUS "${_pfx}_GIT_REV        = ${${_pfx}_GIT_REV}")
#message(STATUS "${_pfx}_GIT_REV_SHORT  = ${${_pfx}_GIT_REV_SHORT}")
#message(STATUS "${_pfx}_VERSION        = ${${_pfx}_VERSION}")
#message(STATUS "${_pfx}_VERSION_INT    = ${${_pfx}_VERSION_INT}")
#message(STATUS "${_pfx}_VERSION_STRING = ${${_pfx}_VERSION_STRING}")

message(STATUS "Writing version ${${_pfx}_VERSION_STRING}")
