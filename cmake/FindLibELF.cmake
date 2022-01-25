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

find_package(PkgConfig QUIET)
pkg_check_modules(PKGCFG_LIBELF QUIET libelf)

find_path(LIBELF_INCLUDE_DIRS NAMES "libelf.h"
          HINTS $ENV{LIBELF_HOME}/include ${PKGCFG_LIBELF_INCLUDE_DIRS})

find_library(LIBELF_LIBRARIES NAMES elf "elf"
             HINTS $ENV{LIBELF_HOME}/lib ${PKGCFG_LIBELF_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibELF
    REQUIRED_VARS LIBELF_INCLUDE_DIRS LIBELF_LIBRARIES
    VERSION_VAR   PKGCFG_LIBELF_VERSION)

mark_as_advanced(LIBELF_INCLUDE_DIRS LIBELF_LIBRARIES)

#message(STATUS "LIBELF_FOUND        " ${LIBELF_FOUND})
#message(STATUS "LIBELF_INCLUDE_DIRS " ${LIBELF_INCLUDE_DIRS})
#message(STATUS "LIBELF_LIBRARIES    " ${LIBELF_LIBRARIES})
