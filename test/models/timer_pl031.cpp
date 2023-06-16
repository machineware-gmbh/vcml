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

class pl031_test : public test_base
{
public:
    timers::pl031 pl031;
    tlm_initiator_socket out;
    gpio_target_socket irq;

    enum : u64 {
        NRF51_START = 0x0,
        NRF51_STOP = 0x4,
        NRF51_COUNT = 0x8,
        NRF51_CLEAR = 0xc,
        NRF51_SHUTDOWN = 0x10,
        NRF51_CAPTURE0 = 0x40,
        NRF51_COMPARE0 = 0x140,
        NRF51_SHORTS = 0x200,
        NRF51_INTENSET = 0x304,
        NRF51_INTENCLR = 0x308,
        NRF51_MODE = 0x504,
        NRF51_BITMODE = 0x508,
        NRF51_PRESCALER = 0x510,
        NRF51_CC0 = 0x540,
    };

    pl031_test(const sc_module_name& nm):
        test_base(nm), pl031("pl031"), out("out"), irq("irq") {
        out.bind(pl031.in);
        pl031.irq.bind(irq);
        rst.bind(pl031.rst);
        pl031.clk.stub(1 * Hz);
        EXPECT_STREQ(pl031.kind(), "vcml::timers::pl031");
    }

    virtual void run_test() override {
        wait(SC_ZERO_TIME);
        test_amba_ids();
        wait(SC_ZERO_TIME);
        test_time_date();
        wait(SC_ZERO_TIME);
        test_alarm_irq();
    }

    constexpr u64 pl031_pid(size_t i) { return 0xfe0 + i * 4; }
    constexpr u64 pl031_cid(size_t i) { return 0xff0 + i * 4; }

    void test_amba_ids() {
        u32 pid = 0;
        for (size_t i = 0; i < 4; i++) {
            u32 data = 0;
            ASSERT_OK(out.readw(pl031_pid(i), data));
            pid |= data << (i * 8);
        }

        u32 cid = 0;
        for (size_t i = 0; i < 4; i++) {
            u32 data = 0;
            ASSERT_OK(out.readw(pl031_cid(i), data));
            cid |= data << (i * 8);
        }

        EXPECT_EQ(pid, timers::pl031::AMBA_PID);
        EXPECT_EQ(cid, timers::pl031::AMBA_CID);
    }

    enum : u64 {
        PL031_DR = 0x0,
        PL031_MR = 0x4,
        PL031_LR = 0x8,
        PL031_CR = 0xc,
        PL031_IMSC = 0x10,
        PL031_RIS = 0x14,
        PL031_MIS = 0x18,
        PL031_ICR = 0x1c,
    };

    void test_time_date() {
        u32 data, data2;
        ASSERT_OK(out.readw(PL031_CR, data));
        EXPECT_EQ(data, 1);
        ASSERT_OK(out.readw(PL031_DR, data));
        wait(10, SC_SEC);
        ASSERT_OK(out.readw(PL031_DR, data2));
        EXPECT_EQ(data2 - data, 10);
    }

    void test_alarm_irq() {
        u32 data;
        ASSERT_OK(out.readw(PL031_CR, data));
        EXPECT_EQ(data, 1);
        ASSERT_OK(out.writew(PL031_IMSC, 1u));
        ASSERT_FALSE(irq);
        ASSERT_OK(out.readw(PL031_DR, data));
        ASSERT_OK(out.writew(PL031_MR, data + 10));

        ASSERT_FALSE(irq);
        wait(8, SC_SEC);
        ASSERT_FALSE(irq);
        wait(3, SC_SEC);

        ASSERT_TRUE(irq);
        ASSERT_OK(out.readw(PL031_RIS, data));
        EXPECT_EQ(data, 1);
        ASSERT_OK(out.readw(PL031_MIS, data));
        EXPECT_EQ(data, 1);

        ASSERT_OK(out.writew(PL031_IMSC, 0u));
        ASSERT_FALSE(irq);
        ASSERT_OK(out.readw(PL031_MIS, data));
        EXPECT_EQ(data, 0);

        ASSERT_OK(out.writew(PL031_IMSC, 1u));
        ASSERT_TRUE(irq);
        ASSERT_OK(out.readw(PL031_MIS, data));
        EXPECT_EQ(data, 1);

        ASSERT_OK(out.writew(PL031_ICR, 1u));
        ASSERT_FALSE(irq);
        ASSERT_OK(out.readw(PL031_MIS, data));
        EXPECT_EQ(data, 0);
        ASSERT_OK(out.readw(PL031_RIS, data));
        EXPECT_EQ(data, 0);
    }
};

TEST(timer, pl031) {
    pl031_test test("test");
    sc_core::sc_start();
}
