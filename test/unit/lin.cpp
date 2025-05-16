/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

#include "vcml/protocols/lin.h"

#define EXPECT_LINOK(call)   EXPECT_EQ((call), LIN_SUCCESS)
#define EXPECT_TIMEOUT(call) EXPECT_EQ((call), LIN_TIMEOUT_ERROR)

TEST(lin, to_string) {
    lin_payload tx;
    tx.linid = 30;
    tx.data[0] = 0x12;
    tx.data[1] = 0x34;
    tx.status = LIN_BIT_ERROR;

    std::stringstream s1;
    s1 << tx.status;
    EXPECT_EQ(s1.str(), "LIN_BIT_ERROR");

    std::stringstream s3;
    s3 << tx;
    EXPECT_EQ(s3.str(), "LIN 30 [12 34] (LIN_BIT_ERROR)");
}

TEST(lin, payload_size) {
    lin_payload tx2;
    tx2.linid = 10;
    EXPECT_EQ(tx2.size(), 2);

    lin_payload tx4;
    tx4.linid = 32;
    EXPECT_EQ(tx4.size(), 4);

    lin_payload tx8;
    tx8.linid = 48;
    EXPECT_EQ(tx8.size(), 8);
}

class lin_bench : public test_base, public lin_host
{
public:
    lin_initiator_socket lin_out;
    lin_base_initiator_socket lin_out_h;
    lin_base_target_socket lin_in_h;
    lin_target_socket lin_in;

    lin_initiator_array<> lin_array_out;
    lin_target_array<> lin_array_in;

    lin_bench(const sc_module_name& nm):
        test_base(nm),
        lin_host(),
        lin_out("lin_out"),
        lin_out_h("lin_out_h"),
        lin_in_h("lin_in_h"),
        lin_in("lin_in"),
        lin_array_out("lin_array_out"),
        lin_array_in("lin_array_in") {
        lin_bind(*this, "lin_out", *this, "lin_out_h");
        lin_bind(*this, "lin_in_h", *this, "lin_in");
        lin_bind(*this, "lin_out_h", *this, "lin_in_h");

        lin_stub(*this, "lin_array_out", 5);
        lin_stub(*this, "lin_array_in", 6);

        // test binding multiple targets to one initiator
        lin_bind(*this, "lin_out", *this, "lin_array_in", 43);
        lin_bind(*this, "lin_out", *this, "lin_array_in", 44);
        lin_bind(*this, "lin_out", *this, "lin_array_in", 45);
        lin_bind(*this, "lin_out", *this, "lin_array_in", 46);

        // did the port get created?
        EXPECT_TRUE(find_object("test.lin_array_out[5]"));
        EXPECT_TRUE(find_object("test.lin_array_in[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("test.lin_array_out[5]_stub"));
        EXPECT_TRUE(find_object("test.lin_array_in[6]_stub"));

        // test strings
        EXPECT_STREQ(lin_out.kind(), "vcml::lin_initiator_socket");
        EXPECT_STREQ(lin_out_h.kind(), "vcml::lin_base_initiator_socket");
        EXPECT_STREQ(lin_in_h.kind(), "vcml::lin_base_target_socket");
        EXPECT_STREQ(lin_in.kind(), "vcml::lin_target_socket");
    }

    virtual void lin_receive(const lin_target_socket& s,
                             lin_payload& tx) override {
        switch (tx.linid) {
        case 12:
            EXPECT_EQ(tx.size(), 2);
            EXPECT_EQ(tx.status, LIN_INCOMPLETE);
            EXPECT_EQ(tx.data[0], 0x12);
            EXPECT_EQ(tx.data[1], 0x34);
            EXPECT_EQ(tx.data[2], 0x00);
            EXPECT_EQ(tx.data[3], 0x00);
            tx.data[0] = 0xff;
            tx.data[1] = 0xee;
            tx.status = LIN_SUCCESS;
        }
    }

    virtual void run_test() override {
        u8 data[] = { 0x12, 0x34, 0x56, 0x78 };

        EXPECT_LINOK(lin_out.send(12, data));
        EXPECT_EQ(data[0], 0xff);
        EXPECT_EQ(data[1], 0xee);
        EXPECT_EQ(data[2], 0x56);
        EXPECT_EQ(data[3], 0x78);

        EXPECT_TIMEOUT(lin_out.send(13, nullptr));
    }
};

TEST(lin, simulate) {
    lin_bench test("test");
    sc_core::sc_start();
}
