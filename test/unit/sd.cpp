/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
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

    sd_initiator_array<> sd_out_arr;
    sd_target_array<> sd_in_arr;

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

TEST(sd, opcode_strings) {
    EXPECT_STREQ(sd_opcode_str(0), "GO_IDLE_STATE");
    EXPECT_STREQ(sd_opcode_str(17), "READ_SINGLE_BLOCK");
    EXPECT_STREQ(sd_opcode_str(24), "WRITE_BLOCK");
    EXPECT_STREQ(sd_opcode_str(55), "APP_CMD");
    EXPECT_STREQ(sd_opcode_str(21), "UNKNOWN");
    EXPECT_STREQ(sd_opcode_str(41, true), "SD_SEND_OP_COND");
    EXPECT_STREQ(sd_opcode_str(51, true), "SEND_SCR");
    EXPECT_STREQ(sd_opcode_str(17, true), "UNKNOWN");

    std::set<string> names;
    for (u8 opcode = 0; opcode < 64; opcode++) {
        for (bool appcmd : { false, true }) {
            string name = sd_opcode_str(opcode, appcmd);
            EXPECT_FALSE(name.empty());
            if (name != "UNKNOWN") {
                EXPECT_TRUE(names.insert(name + (appcmd ? "/A" : "")).second)
                    << "duplicate name " << name;
            }
        }
    }
}

TEST(sd, status_strings) {
    EXPECT_STREQ(sd_status_str(SD_INCOMPLETE), "SD_INCOMPLETE");
    EXPECT_STREQ(sd_status_str(SD_OK), "SD_OK");
    EXPECT_STREQ(sd_status_str(SD_OK_TX_RDY), "SD_OK_TX_RDY");
    EXPECT_STREQ(sd_status_str(SD_OK_RX_RDY), "SD_OK_RX_RDY");
    EXPECT_STREQ(sd_status_str(SD_ERR_CRC), "SD_ERR_CRC");
    EXPECT_STREQ(sd_status_str(SD_ERR_ARG), "SD_ERR_ARG");
    EXPECT_STREQ(sd_status_str(SD_ERR_ILLEGAL), "SD_ERR_ILLEGAL");

    EXPECT_STREQ(sd_status_str(SDTX_INCOMPLETE), "SDTX_INCOMPLETE");
    EXPECT_STREQ(sd_status_str(SDTX_OK), "SDTX_OK");
    EXPECT_STREQ(sd_status_str(SDTX_OK_BLK_DONE), "SDTX_OK_BLK_DONE");
    EXPECT_STREQ(sd_status_str(SDTX_OK_COMPLETE), "SDTX_OK_COMPLETE");
    EXPECT_STREQ(sd_status_str(SDTX_ERR_ILLEGAL), "SDTX_ERR_ILLEGAL");

    EXPECT_STREQ(sd_status_str(SDRX_INCOMPLETE), "SDRX_INCOMPLETE");
    EXPECT_STREQ(sd_status_str(SDRX_OK), "SDRX_OK");
    EXPECT_STREQ(sd_status_str(SDRX_OK_BLK_DONE), "SDRX_OK_BLK_DONE");
    EXPECT_STREQ(sd_status_str(SDRX_OK_COMPLETE), "SDRX_OK_COMPLETE");
    EXPECT_STREQ(sd_status_str(SDRX_ERR_CRC), "SDRX_ERR_CRC");
    EXPECT_STREQ(sd_status_str(SDRX_ERR_INT), "SDRX_ERR_INT");
    EXPECT_STREQ(sd_status_str(SDRX_ERR_ILLEGAL), "SDRX_ERR_ILLEGAL");
}

TEST(sd, to_string) {
    sd_command cmd;
    sd_reset(cmd);
    cmd.opcode = 17;
    cmd.argument = 0x200;
    EXPECT_EQ(to_string(cmd),
              "SD-CMD17 READ_SINGLE_BLOCK (00000200) (SD_INCOMPLETE)");

    cmd.response[0] = 0x11;
    cmd.response[1] = 0xab;
    cmd.resp_len = 2;
    cmd.status = SD_OK_TX_RDY;
    EXPECT_EQ(to_string(cmd),
              "SD-CMD17 READ_SINGLE_BLOCK (00000200) [11 ab] (SD_OK_TX_RDY)");

    sd_reset(cmd);
    cmd.opcode = 41;
    cmd.appcmd = true;
    cmd.status = SD_ERR_ILLEGAL;
    EXPECT_EQ(to_string(cmd),
              "SD-ACMD41 SD_SEND_OP_COND (00000000) (SD_ERR_ILLEGAL)");

    sd_data data;
    sd_init_read(data);
    EXPECT_FALSE(success(data));
    EXPECT_FALSE(failed(data));
    EXPECT_EQ(to_string(data), "SD-DATA read (SDTX_INCOMPLETE)");
    data.data = 0x5a;
    data.status.read = SDTX_OK;
    EXPECT_TRUE(success(data));
    EXPECT_EQ(to_string(data), "SD-DATA read [5a] (SDTX_OK)");

    sd_init_write(data);
    data.data = 0xa5;
    data.status.write = SDRX_ERR_CRC;
    EXPECT_TRUE(failed(data));
    EXPECT_EQ(to_string(data), "SD-DATA write [a5] (SDRX_ERR_CRC)");
}

TEST(sd, sockets) {
    sd_harness test("sd");
    EXPECT_STREQ(test.sd_out_h.kind(), "vcml::sd_base_initiator_socket");
    EXPECT_STREQ(test.sd_in_h.kind(), "vcml::sd_base_target_socket");
    EXPECT_STREQ(test.sd_out.kind(), "vcml::sd_initiator_socket");
    EXPECT_STREQ(test.sd_in.kind(), "vcml::sd_target_socket");
    sc_core::sc_start();
}
