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

#include "testing.h"

enum : address_space {
    VCML_AS_TEST = VCML_AS_DEFAULT + 1,
};

class sd_harness : public test_base, public sd_host
{
public:
    sd_initiator_socket sd_out;
    sd_target_socket sd_in;

    sd_base_initiator_socket sd_out_h;
    sd_base_target_socket sd_in_h;

    sd_initiator_socket_array<> sd_out_arr;
    sd_target_socket_array<> sd_in_arr;

    sd_harness(const sc_module_name& nm):
        test_base(nm),
        sd_host(),
        sd_out("sd_out"),
        sd_in("sd_in", VCML_AS_TEST),
        sd_out_h("sd_out_h"),
        sd_in_h("sd_in_h"),
        sd_out_arr("sd_out_arr"),
        sd_in_arr("sd_in_arr") {
        // test hierarchy binding
        sd_out.bind(sd_out_h);
        sd_in_h.bind(sd_in);
        sd_out_h.bind(sd_in_h);

        // test stubbing
        sd_out_arr[28].stub();
        sd_in_arr[29].stub();

        EXPECT_TRUE(find_object("sd.sd_out_arr[28]_stub"));
        EXPECT_TRUE(find_object("sd.sd_in_arr[29]_stub"));

        auto initiators = all_sd_initiator_sockets();
        auto targets    = all_sd_target_sockets();
        auto sockets    = all_sd_target_sockets(VCML_AS_TEST);

        EXPECT_EQ(initiators.size(), 2) << "sd initiators did not register";
        EXPECT_EQ(targets.size(), 2) << "sd targets did not register";
        EXPECT_EQ(sockets.size(), 1) << "sd targets in wrong address space";
    }

    virtual void sd_transport(const sd_target_socket& socket,
                              sd_command& cmd) override {
        EXPECT_EQ(socket.as, VCML_AS_TEST);
        cmd.status      = SD_OK;
        cmd.response[0] = cmd.argument * 10;
    }

    virtual void sd_transport(const sd_target_socket& socket,
                              sd_data& data) override {
        EXPECT_EQ(socket.as, VCML_AS_TEST);
        EXPECT_EQ(data.mode, SD_READ);
        data.data *= 10;
        data.status.read = SDTX_OK;
    }

    virtual void run_test() override {
        for (vcml::u8 i = 0; i < 10; i++) {
            wait(1, sc_core::SC_SEC);
            sd_command cmd = {};
            cmd.opcode     = 0;
            cmd.argument   = i;
            cmd.status     = SD_INCOMPLETE;
            sd_out->sd_transport(cmd);
            EXPECT_TRUE(success(cmd));
            EXPECT_EQ(cmd.response[0], i * 10);

            sd_data data     = {};
            data.mode        = SD_READ;
            data.data        = i;
            data.status.read = SDTX_INCOMPLETE;
            sd_out->sd_transport(data);
            EXPECT_TRUE(success(data));
            EXPECT_EQ(data.data, i * 10);
        }
    }
};

TEST(sd, sockets) {
    sd_harness test("sd");
    sc_core::sc_start();
}
