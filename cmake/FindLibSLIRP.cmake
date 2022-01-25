 ##############################################################################
 #                                                                            #
 # Copyright 2021 Jan Henrik Weinstock                                        #
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
pkg_check_modules(PKGCFG_SLIRP QUIET slirp)

find_path(LIBSLIRP_INCLUDE_DIRS NAMES "libslirp.h"
          HINTS $ENV{LIBSLIRP_HOME}/include/slirp ${PKGCFG_SLIRP_INCLUDE_DIRS})

find_library(LIBSLIRP_LIBRARIES NAMES "slirp"
             HINTS $ENV{LIBSLIRP_HOME}/lib ${PKGCFG_SLIRP_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibSLIRP
    REQUIRED_VARS LIBSLIRP_INCLUDE_DIRS LIBSLIRP_LIBRARIES
    VERSION_VAR   PKGCFG_SLIRP_VERSION)

mark_as_advanced(LIBSLIRP_INCLUDE_DIRS LIBSLIRP_LIBRARIES)

#message(STATUS "LIBSLIRP_FOUND        " ${LIBSLIRP_FOUND})
#message(STATUS "LIBSLIRP_INCLUDE_DIRS " ${LIBSLIRP_INCLUDE_DIRS})
#message(STATUS "LIBSLIRP_LIBRARIES    " ${LIBSLIRP_LIBRARIES})
