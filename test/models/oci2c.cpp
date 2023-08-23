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

using oci2c = vcml::i2c::oci2c;

// oci2c registers are always 8bit wide, but we can test with 8/16/32bit data
// register accesses by specifying a register shift (e.g. u32 -> shift 2)
template <typename DATA, const u8 REG_SHIFT>
class oci2c_bench : public test_base, public i2c_host
{
public:
    constexpr DATA i2c_addr_w(u8 addr) { return (DATA)addr << 1; }
    constexpr DATA i2c_addr_r(u8 addr) { return (DATA)addr << 1 | 1; }

    enum address : u32 {
        PRERLO = 0u,
        PRERHI = 1u,
        CTR = 2u,
        RXR = 3u,
        TXR = 3u,
        SR = 4u,
        CR = 4u,
    };

    oci2c model;

    tlm_initiator_socket out;
    gpio_target_socket irq;
    i2c_target_socket i2c;

    MOCK_METHOD(i2c_response, i2c_start,
                (const i2c_target_socket&, tlm_command));
    MOCK_METHOD(i2c_response, i2c_stop, (const i2c_target_socket&));
    MOCK_METHOD(i2c_response, i2c_read, (const i2c_target_socket&, u8& data));
    MOCK_METHOD(i2c_response, i2c_write, (const i2c_target_socket&, u8 data));

    oci2c_bench(const sc_module_name& nm):
        test_base(nm),
        i2c_host(),
        model("oci2c", REG_SHIFT),
        out("out"),
        irq("irq"),
        i2c("i2c") {
        i2c.set_address(42);
        out.bind(model.in);
        rst.bind(model.rst);
        clk.bind(model.clk);
        model.irq.bind(irq);
        model.i2c.bind(i2c);
    }

    tlm_response_status reg_read(u32 addr, DATA& val) {
        return out.readw(addr * sizeof(DATA), val);
    }

    tlm_response_status reg_write(u32 addr, DATA val) {
        return out.writew(addr * sizeof(DATA), val);
    }

    void test_setup() {
        // test that interrupts are reset
        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irq.read()) << "irq not reset";
        EXPECT_FALSE(model.irq.read()) << "irq not reset";
        EXPECT_EQ(model.bus_hz(), 0) << "bus clock not reset";

        // program prescaler for 100kHz operation
        hz_t tgthz = 100 * kHz;
        u16 prescaler = clk.read() / (5 * tgthz) - 1;
        EXPECT_OK(reg_write(PRERHI, prescaler >> 8));
        EXPECT_OK(reg_write(PRERLO, prescaler));
        EXPECT_EQ(model.bus_hz(), 0);
        EXPECT_OK(reg_write(CTR, oci2c::CTR_EN));
        EXPECT_EQ(model.bus_hz(), tgthz);
    }

    void test_write() {
        DATA data = 0;

        // setup write operation
        ASSERT_OK(reg_write(TXR, i2c_addr_w(42)));
        EXPECT_CALL(*this, i2c_start(_, TLM_WRITE_COMMAND))
            .Times(1)
            .WillOnce(Return(I2C_ACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_STA | oci2c::CMD_WR));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_TRUE(data & oci2c::SR_IF) << "interrupt flag not set";
        EXPECT_FALSE(irq) << "interrupt received despite masked";
        ASSERT_OK(reg_write(CR, oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_FALSE(data & oci2c::SR_IF) << "interrupt flag not cleared";

        // perform write operation (with interrupts)
        EXPECT_OK(reg_write(CTR, oci2c::CTR_EN | oci2c::CTR_IEN));
        EXPECT_OK(reg_write(TXR, 21));
        EXPECT_CALL(*this, i2c_write(_, 21))
            .Times(1)
            .WillOnce(Return(I2C_ACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_WR));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, oci2c::SR_IF) << "unexpected status reported";
        EXPECT_TRUE(irq) << "no interrupt received";
        ASSERT_OK(reg_write(CR, oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, 0) << "unexpected status received";
        EXPECT_FALSE(irq) << "interrupt not cleared";

        // finish write
        EXPECT_CALL(*this, i2c_stop(_)).Times(1).WillOnce(Return(I2C_ACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_STO | oci2c::CMD_IACK));
        EXPECT_TRUE(irq) << "interrupt after stop not received";
        ASSERT_OK(reg_read(SR, data));
        EXPECT_EQ(data, oci2c::SR_IF) << "unexpected status reported";
        ASSERT_OK(reg_write(CR, oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, 0) << "unexpected status received";
        EXPECT_FALSE(irq) << "interrupt not cleared";
    }

    void test_read() {
        DATA data = 0;

        // disable interrupts
        EXPECT_OK(reg_write(CTR, oci2c::CTR_EN));

        // setup transfer
        ASSERT_OK(reg_write(TXR, i2c_addr_r(42)));
        EXPECT_CALL(*this, i2c_start(_, TLM_READ_COMMAND))
            .Times(1)
            .WillOnce(Return(I2C_ACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_STA | oci2c::CMD_WR));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, oci2c::SR_IF) << "interrupt flag not set";

        // trigger transfer
        EXPECT_CALL(*this, i2c_read(_, _))
            .Times(1)
            .WillOnce(DoAll(SetArgReferee<1>(10), Return(I2C_ACK)));
        ASSERT_OK(reg_write(CR, oci2c::CMD_RD));
        EXPECT_OK(reg_read(SR, data));
        EXPECT_EQ(data, oci2c::SR_IF) << "unexpected status reported";
        ASSERT_OK(reg_read(RXR, data));
        EXPECT_EQ(data, 10) << "invalid data received";

        // finish transfer
        EXPECT_CALL(*this, i2c_stop(_)).Times(1).WillOnce(Return(I2C_ACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_STO | oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        EXPECT_EQ(data, oci2c::SR_IF) << "unexpected status reported";
        ASSERT_OK(reg_write(CR, oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, 0) << "unexpected status received";
    }

    void test_error() {
        DATA data = 0;

        // disable interrupts
        EXPECT_OK(reg_write(CTR, oci2c::CTR_EN));

        // setup transfer
        ASSERT_OK(reg_write(TXR, i2c_addr_r(42)));
        EXPECT_CALL(*this, i2c_start(_, TLM_READ_COMMAND))
            .Times(1)
            .WillOnce(Return(I2C_NACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_STA | oci2c::CMD_WR));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, oci2c::SR_NACK | oci2c::SR_IF);

        // finish transfer
        EXPECT_CALL(*this, i2c_stop(_)).Times(1).WillOnce(Return(I2C_NACK));
        ASSERT_OK(reg_write(CR, oci2c::CMD_STO | oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        EXPECT_EQ(data, oci2c::SR_NACK | oci2c::SR_IF);
        ASSERT_OK(reg_write(CR, oci2c::CMD_IACK));
        ASSERT_OK(reg_read(SR, data));
        ASSERT_EQ(data, 0) << "unexpected status received";
    }

    virtual void run_test() override {
        test_setup();
        test_write();
        test_read();
        test_error();
    }
};

TEST(oci2c, simulate) {
    oci2c_bench<u32, 2> test("bench");
    sc_core::sc_start();
}
