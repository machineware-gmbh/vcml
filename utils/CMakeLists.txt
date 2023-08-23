 ##############################################################################
 #                                                                            #
 # Copyright (C) 2022 MachineWare GmbH                                        #
 # All Rights Reserved                                                        #
 #                                                                            #
 # This is work is licensed under the terms described in the LICENSE file     #
 # found in the root directory of this source tree.                           #
 #                                                                            #
 ##############################################################################

if(TAP_FOUND)
    add_executable(vcml-tapctl tapctl.c)
    install(TARGETS vcml-tapctl DESTINATION bin)
    install(PROGRAMS tapnet DESTINATION bin RENAME vcml-tapnet)
endif()
