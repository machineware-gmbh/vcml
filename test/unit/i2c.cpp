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

#include "vcml/protocols/i2c.h"

#define EXPECT_ACK(call)  EXPECT_EQ((call), I2C_ACK)
#define EXPECT_NACK(call) EXPECT_EQ((call), I2C_NACK)

MATCHER_P(i2c_match_address, addr, "Matches an i2c socket address") {
    return arg.address() == addr;
}

TEST(i2c, to_string) {
    i2c_payload tx;
    tx.cmd = I2C_START;
    tx.resp = I2C_ACK;
    tx.data = 0xff;

    std::stringstream s1;
    s1 << tx.cmd;
    EXPECT_EQ(s1.str(), "I2C_START");

    std::stringstream s2;
    s2 << tx.resp;
    EXPECT_EQ(s2.str(), "I2C_ACK");

    std::stringstream s3;
    s3 << tx;
    EXPECT_EQ(s3.str(), "I2C_START [ff] (I2C_ACK)");
}

TEST(i2c, result) {
    i2c_payload ok;
    ok.cmd = I2C_DATA;
    ok.resp = I2C_ACK;

    i2c_payload err;
    err.cmd = I2C_DATA;
    err.resp = I2C_NACK;

    EXPECT_TRUE(success(ok));
    EXPECT_FALSE(failed(ok));

    EXPECT_FALSE(success(err));
    EXPECT_TRUE(failed(err));
}

class i2c_bench : public test_base, public i2c_host
{
public:
    i2c_initiator_socket i2c_out;
    i2c_base_initiator_socket i2c_out_h;
    i2c_base_target_socket i2c_in_h;
    i2c_target_socket i2c_in;

    i2c_initiator_array i2c_array_out;
    i2c_target_array i2c_array_in;

    i2c_bench(const sc_module_name& nm):
        test_base(nm),
        i2c_host(),
        i2c_out("i2c_out"),
        i2c_out_h("i2c_out_h"),
        i2c_in_h("i2c_in_h"),
        i2c_in("i2c_in"),
        i2c_array_out("i2c_array_out"),
        i2c_array_in("i2c_array_in") {
        i2c_set_address(*this, "i2c_in", 42);
        EXPECT_EQ(i2c_in.address(), 42);

        i2c_bind(*this, "i2c_out", *this, "i2c_out_h");
        i2c_bind(*this, "i2c_in_h", *this, "i2c_in");
        i2c_bind(*this, "i2c_out_h", *this, "i2c_in_h");

        i2c_stub(*this, "i2c_array_out", 5);
        i2c_stub(*this, "i2c_array_in", 6);

        // test binding multiple targets to one initiator
        i2c_bind(*this, "i2c_out", *this, "i2c_array_in", 43);
        i2c_bind(*this, "i2c_out", *this, "i2c_array_in", 44);
        i2c_bind(*this, "i2c_out", *this, "i2c_array_in", 45);
        i2c_bind(*this, "i2c_out", *this, "i2c_array_in", 46);

        i2c_set_address(*this, "i2c_array_in", 43, 43);
        i2c_set_address(*this, "i2c_array_in", 44, 44);
        i2c_set_address(*this, "i2c_array_in", 45, 45);
        i2c_set_address(*this, "i2c_array_in", 46, 46);

        // did the port get created?
        EXPECT_TRUE(find_object("i2c.i2c_array_out[5]"));
        EXPECT_TRUE(find_object("i2c.i2c_array_in[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("i2c.i2c_array_out[5]_stub"));
        EXPECT_TRUE(find_object("i2c.i2c_array_in[6]_stub"));
    }

    MOCK_METHOD(i2c_response, i2c_start,
                (const i2c_target_socket&, tlm_command));
    MOCK_METHOD(i2c_response, i2c_stop, (const i2c_target_socket&));
    MOCK_METHOD(i2c_response, i2c_read, (const i2c_target_socket&, u8&));
    MOCK_METHOD(i2c_response, i2c_write, (const i2c_target_socket&, u8));

    virtual void run_test() override {
        // test starting a read transfer
        EXPECT_CALL(*this, i2c_start(i2c_match_address(42), TLM_READ_COMMAND))
            .Times(1)
            .WillOnce(Return(I2C_ACK));
        EXPECT_ACK(i2c_out.start(42, TLM_READ_COMMAND));

        // write to non-existent address
        EXPECT_NACK(i2c_out.start(99, TLM_READ_COMMAND));

        // test starting a write transfer
        EXPECT_CALL(*this, i2c_start(i2c_match_address(44), TLM_WRITE_COMMAND))
            .Times(1)
            .WillOnce(Return(I2C_ACK));
        EXPECT_ACK(i2c_out.start(44, TLM_WRITE_COMMAND));

        // does the data get received?
        u8 data = 0xab;
        EXPECT_CALL(*this, i2c_write(i2c_match_address(44), data))
            .Times(3)
            .WillRepeatedly(Return(I2C_ACK));
        EXPECT_ACK(i2c_out.transport(data));
        EXPECT_ACK(i2c_out.transport(data));
        EXPECT_ACK(i2c_out.transport(data));

        // can we stop the transfer?
        EXPECT_CALL(*this, i2c_stop(i2c_match_address(44)))
            .Times(1)
            .WillOnce(Return(I2C_ACK));
        EXPECT_ACK(i2c_out.stop());
        EXPECT_CALL(*this, i2c_write(_, data)).Times(0);
        EXPECT_NACK(i2c_out.transport(data));
    }
};

TEST(i2c, simulate) {
    i2c_bench bench("i2c");
    sc_core::sc_start();
}
