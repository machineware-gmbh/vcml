/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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

    sd_initiator_array sd_out_arr;
    sd_target_array sd_in_arr;

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
        sd_bind(*this, "sd_out", *this, "sd_out_h");
        sd_bind(*this, "sd_in_h", *this, "sd_in");
        sd_bind(*this, "sd_out_h", *this, "sd_in_h");

        // test stubbing
        sd_stub(*this, "sd_out_arr", 28);
        sd_stub(*this, "sd_in_arr", 29);

        EXPECT_TRUE(find_object("sd.sd_out_arr[28]_stub"));
        EXPECT_TRUE(find_object("sd.sd_in_arr[29]_stub"));
    }

    virtual void sd_transport(const sd_target_socket& socket,
                              sd_command& cmd) override {
        EXPECT_EQ(socket.as, VCML_AS_TEST);
        cmd.status = SD_OK;
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
            cmd.opcode = 0;
            cmd.argument = i;
            cmd.status = SD_INCOMPLETE;
            sd_out->sd_transport(cmd);
            EXPECT_TRUE(success(cmd));
            EXPECT_EQ(cmd.response[0], i * 10);

            sd_data data = {};
            data.mode = SD_READ;
            data.data = i;
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
