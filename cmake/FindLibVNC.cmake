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
pkg_check_modules(PKGCFG_VNC QUIET libvncserver)

find_path(LIBVNC_INCLUDE_DIRS NAMES "rfb/rfb.h"
          HINTS $ENV{LIBVNC_HOME}/include ${PKGCFG_VNC_INCLUDE_DIRS})

find_library(LIBVNC_LIBRARIES NAMES "vncserver"
             HINTS $ENV{LIBVNC_HOME}/lib ${PKGCFG_VNC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibVNC
    REQUIRED_VARS LIBVNC_INCLUDE_DIRS LIBVNC_LIBRARIES
    VERSION_VAR   PKGCFG_VNC_VERSION)

mark_as_advanced(LIBVNC_INCLUDE_DIRS LIBVNC_LIBRARIES)

#message(STATUS "LIBVNC_FOUND        " ${LIBVNC_FOUND})
#message(STATUS "LIBVNC_INCLUDE_DIRS " ${LIBVNC_INCLUDE_DIRS})
#message(STATUS "LIBVNC_LIBRARIES    " ${LIBVNC_LIBRARIES})
