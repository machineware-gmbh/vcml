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

if(EXISTS $ENV{LIBELF_HOME})
    set(LIBELF_INCLUDE_DIRS $ENV{LIBELF_HOME}/include
                            $ENV{LIBELF_HOME}/include/libelf)
    set(LIBELF_LIBRARIES    $ENV{LIBELF_HOME}/lib/libelf.a)
else()
    find_path(LIBELF_INCLUDE_DIRS NAMES libelf.h
              HINTS /usr/include /usr/include/libelf /opt/libelf/include)

    find_library(LIBELF_LIBRARIES NAMES elf
                 HINTS /usr/lib /opt/libelf/lib LD_LIBRARY_PATH)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibElf DEFAULT_MSG
                                  LIBELF_LIBRARIES
                                  LIBELF_INCLUDE_DIRS)

mark_as_advanced(LIBELF_INCLUDE_DIRS LIBELF_LIBRARIES)

#message(STATUS "LIBELF_FOUND        " ${LIBELF_FOUND})
#message(STATUS "LIBELF_INCLUDE_DIRS " ${LIBELF_INCLUDE_DIRS})
#message(STATUS "LIBELF_LIBRARIES    " ${LIBELF_LIBRARIES})
