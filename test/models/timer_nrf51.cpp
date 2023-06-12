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

class nrf51_test : public test_base
{
public:
    timers::nrf51 nrf51;
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

    constexpr u64 nrf51_capture(size_t i) { return NRF51_CAPTURE0 + i * 4; }
    constexpr u64 nrf51_compare(size_t i) { return NRF51_COMPARE0 + i * 4; }
    constexpr u64 nrf51_cc(size_t i) { return NRF51_CC0 + i * 4; }

    nrf51_test(const sc_module_name& nm):
        test_base(nm), nrf51("nrf51"), out("out"), irq("irq") {
        out.bind(nrf51.in);
        nrf51.irq.bind(irq);
        rst.bind(nrf51.rst);
        nrf51.clk.stub(16 * MHz);
        EXPECT_STREQ(nrf51.kind(), "vcml::timers::nrf51");
    }

    virtual void run_test() override {
        u32 data;
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(irq) << "irq did not reset";

        // setup bitmode and prescaler
        ASSERT_OK(out.writew<u32>(NRF51_BITMODE, 3));
        ASSERT_OK(out.writew<u32>(NRF51_PRESCALER, 4)); // 16 MHz / 2^4

        // start time and sample
        ASSERT_OK(out.writew<u32>(NRF51_START, 1));
        wait(1, SC_SEC);
        ASSERT_OK(out.writew<u32>(NRF51_STOP, 1));
        ASSERT_OK(out.writew<u32>(nrf51_capture(1), 1));
        ASSERT_OK(out.readw<u32>(nrf51_cc(1), data));
        ASSERT_EQ(data, 1000000);

        // timer should not count while stopped
        wait(10, SC_SEC);
        ASSERT_OK(out.writew<u32>(nrf51_capture(2), 1));
        ASSERT_OK(out.readw<u32>(nrf51_cc(2), data));
        ASSERT_EQ(data, 1000000);

        // clear timer
        ASSERT_OK(out.writew<u32>(NRF51_CLEAR, 1));
        ASSERT_OK(out.writew<u32>(nrf51_capture(3), 1));
        ASSERT_OK(out.readw<u32>(nrf51_cc(3), data));
        ASSERT_EQ(data, 0);
        ASSERT_FALSE(irq);

        // test interrupts
        ASSERT_OK(out.writew<u32>(NRF51_INTENSET, bit(16)));
        ASSERT_OK(out.writew<u32>(nrf51_cc(0), 2000000));
        ASSERT_OK(out.writew<u32>(nrf51_compare(0), 0));
        ASSERT_OK(out.writew<u32>(NRF51_START, 1));
        ASSERT_FALSE(irq);
        sc_time t = sc_time_stamp();
        wait(irq.default_event());
        ASSERT_TRUE(irq);
        t = sc_time_stamp() - t;
        ASSERT_EQ(t, sc_time(2.0, SC_SEC));
        ASSERT_OK(out.readw<u32>(nrf51_compare(0), data));
        ASSERT_EQ(data, 1);
        ASSERT_OK(out.writew<u32>(nrf51_cc(0), 0));
        ASSERT_OK(out.writew<u32>(nrf51_compare(0), 0));
        ASSERT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(NRF51_START, 0));

        // test counter mode
        ASSERT_OK(out.writew<u32>(NRF51_MODE, 1));
        ASSERT_OK(out.writew<u32>(NRF51_CLEAR, 1));
        ASSERT_OK(out.writew<u32>(NRF51_START, 1));
        for (size_t i = 0; i < 3; i++)
            ASSERT_OK(out.writew<u32>(NRF51_COUNT, 1));
        ASSERT_OK(out.writew<u32>(nrf51_capture(2), 1));
        ASSERT_OK(out.readw<u32>(nrf51_cc(2), data));
        ASSERT_EQ(data, 3);
        ASSERT_OK(out.writew<u32>(nrf51_cc(0), 5));
        ASSERT_OK(out.writew<u32>(nrf51_compare(0), 0));
        ASSERT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(NRF51_COUNT, 1));
        ASSERT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(NRF51_COUNT, 1));
        ASSERT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(nrf51_cc(0), 0));
        ASSERT_OK(out.writew<u32>(nrf51_compare(0), 0));
        ASSERT_FALSE(irq);

        // test counter mode stop
        ASSERT_OK(out.writew<u32>(nrf51_capture(3), 1));
        ASSERT_OK(out.readw<u32>(nrf51_cc(3), data));
        ASSERT_EQ(data, 5);
        ASSERT_OK(out.writew<u32>(NRF51_STOP, 1));
        ASSERT_OK(out.writew<u32>(NRF51_COUNT, 1));
        ASSERT_OK(out.writew<u32>(nrf51_capture(1), 1));
        ASSERT_OK(out.readw<u32>(nrf51_cc(1), data));
        ASSERT_EQ(data, 5);
    }
};

TEST(timer, nrf51) {
    nrf51_test test("test");
    sc_core::sc_start();
}
