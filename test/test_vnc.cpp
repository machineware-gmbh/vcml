/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include <iostream>
#include <cstring>
#include <gtest/gtest.h>
#include "vcml.h"

extern "C" int sc_main(int argc, char** argv) {

    vcml::log_term logger;
    logger.set_level(vcml::LOG_ERROR, vcml::LOG_DEBUG);

    if (argc < 3) {
        // automated testing will not work for this
        puts("usage: ./test_vnc <dir> <port>");
        return 0;
    }

    {
        int port = atoi(argv[2]);
        std::shared_ptr<vcml::debugging::vncserver> vnc =
                vcml::debugging::vncserver::lookup(port);
        vcml::log_debug("use count = %d", vnc.use_count());

        bool running = true;
        while (running) {/*
            if (vnc->has_input()) {
                vcml::u8 key = vnc->get_input();
                printf("received key %d (0x%02x) '%c'\n", (int)key, (int)key, (char)key);

                switch (key) {
                case '1':
                    vnc->change_resolution(640, 480);
                    break;
                case '2':
                    vnc->change_resolution(800, 600);
                    break;
                case '3':
                    vnc->change_resolution(1024, 768);
                    break;

                case 0x1b: // escape
                    running = false;
                    break;
                default:
                    // do nothing
                    break;
                }
            }

            usleep(100);
            */
        }
    }

    vcml::log_debug("end of program");
    return 0;
}
