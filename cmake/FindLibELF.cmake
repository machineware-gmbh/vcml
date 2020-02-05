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

find_path(LIBELF_INCLUDE_DIRS NAMES "libelf.h"
          HINTS $ENV{LIBELF_HOME}/include /usr/include)

find_library(LIBELF_LIBRARIES NAMES elf "libelf.a"
             HINTS $ENV{LIBELF_HOME}/lib /usr/lib /lib)

find_library(LIBZ_LIBRARIES NAMES z "libz.a"
             HINTS $ENV{LIBELF_HOME} $ENV{LIBZ_HOME} /usr/lib /lib)

list(APPEND LIBELF_LIBRARIES ${LIBZ_LIBRARIES})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBELF DEFAULT_MSG
                                  LIBELF_LIBRARIES
                                  LIBELF_INCLUDE_DIRS)

mark_as_advanced(LIBELF_INCLUDE_DIRS LIBELF_LIBRARIES)

#message(STATUS "LIBELF_FOUND        " ${LIBELF_FOUND})
#message(STATUS "LIBELF_INCLUDE_DIRS " ${LIBELF_INCLUDE_DIRS})
#message(STATUS "LIBELF_LIBRARIES    " ${LIBELF_LIBRARIES})
