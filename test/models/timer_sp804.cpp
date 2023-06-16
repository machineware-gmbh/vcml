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

class sp804_stim : public test_base
{
public:
    tlm_initiator_socket out;

    gpio_initiator_socket rst_out;

    gpio_target_socket irq1;
    gpio_target_socket irq2;
    gpio_target_socket irqc;

    sp804_stim(const sc_module_name& nm):
        test_base(nm),
        out("out"),
        rst_out("rst_out"),
        irq1("irq1"),
        irq2("irq2"),
        irqc("irqc") {
        rst_out.bind(rst);
    }

    virtual void run_test() override {
        enum addresses : u64 {
            TIMER1_LOAD = 0x00,
            TIMER1_VALUE = 0x04,
            TIMER1_CONTROL = 0x08,
        };

        u32 val = 0x100;
        EXPECT_OK(out.writew(TIMER1_LOAD, val)) << "cannot set counter";

        // force a non-zero test starting time
        wait(100, SC_MS);
        sc_time start = sc_time_stamp();

        // enable timer
        val = 0;
        EXPECT_OK(out.readw(TIMER1_VALUE, val));
        EXPECT_GE(val, 0x100) << "counter changed while disabled";

        val = timers::sp804::timer::CONTROL_ENABLED |
              timers::sp804::timer::CONTROL_IRQEN |
              timers::sp804::timer::CONTROL_ONESHOT |
              timers::sp804::timer::CONTROL_32BIT;
        EXPECT_OK(out.writew(TIMER1_CONTROL, val)) << "cannot write CONTROL";

        wait(irqc.default_event());

        EXPECT_TRUE(irq1) << "irq1 did not fire";
        EXPECT_TRUE(irqc) << "irqc did not propagate from irq1";
        EXPECT_FALSE(irq2) << "irq2 randomly fired";

        EXPECT_EQ(sc_time_stamp(), start + clock_cycles(0x100))
            << "interrupt did not fire at correct time";

        EXPECT_OK(out.readw(TIMER1_VALUE, val)) << "cannot read counter";
        EXPECT_EQ(val, 0) << "counter did not stop at zero";

        // check if resetting works
        EXPECT_OK(out.readw(TIMER1_CONTROL, val)) << "cannot read CONTROL";
        EXPECT_NE(val, 0x20) << "TIMER1_CONTROL changed randomly";

        rst_out = true;
        wait(10, SC_MS);
        rst_out = false;
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(rst);

        val = 0;
        EXPECT_OK(out.readw(TIMER1_CONTROL, val)) << "cannot read CONTROL";
        EXPECT_EQ(val, 0x20) << "TIMER1_CONTROL did not reset";
    }
};

TEST(sp804, main) {
    sp804_stim stim("stim");
    timers::sp804 sp804("sp804");

    stim.out.bind(sp804.in);
    stim.clk.bind(sp804.clk);
    stim.rst.bind(sp804.rst);

    sp804.irq1.bind(stim.irq1);
    sp804.irq2.bind(stim.irq2);
    sp804.irqc.bind(stim.irqc);

    sc_core::sc_start();
}
