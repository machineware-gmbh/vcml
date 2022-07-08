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

find_package(PkgConfig QUIET)
pkg_check_modules(PKGCFG_LIBRFB QUIET librfb)

find_path(LIBRFB_INCLUDE_DIRS NAMES "librfb.h"
          HINTS $ENV{LIBRFB_HOME}/include ${PKGCFG_LIBRFB_INCLUDE_DIRS})

find_library(LIBRFB_LIBRARIES NAMES rfb "rfb"
             HINTS $ENV{LIBRFB_HOME}/lib ${PKGCFG_LIBRFB_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibRFB
    REQUIRED_VARS LIBRFB_INCLUDE_DIRS LIBRFB_LIBRARIES)

mark_as_advanced(LIBRFB_INCLUDE_DIRS LIBRFB_LIBRARIES)

message(DEBUG "LIBRFB_FOUND        " ${LIBRFB_FOUND})
message(DEBUG "LIBRFB_INCLUDE_DIRS " ${LIBRFB_INCLUDE_DIRS})
message(DEBUG "LIBRFB_LIBRARIES    " ${LIBRFB_LIBRARIES})
