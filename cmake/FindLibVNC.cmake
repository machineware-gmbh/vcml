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

find_path(LIBVNC_INCLUDE_DIRS NAMES "rfb/rfb.h"
          HINTS $ENV{LIBVNC_HOME}/include)

find_library(LIBVNC_LIBRARIES NAMES "libvncserver.a"
             HINTS $ENV{LIBVNC_HOME}/lib)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBVNC DEFAULT_MSG
                                  LIBVNC_LIBRARIES
                                  LIBVNC_INCLUDE_DIRS)

mark_as_advanced(LIBVNC_INCLUDE_DIRS LIBVNC_LIBRARIES)

#message(STATUS "LIBVNC_FOUND        " ${LIBVNC_FOUND})
#message(STATUS "LIBVNC_INCLUDE_DIRS " ${LIBVNC_INCLUDE_DIRS})
#message(STATUS "LIBVNC_LIBRARIES    " ${LIBVNC_LIBRARIES})
