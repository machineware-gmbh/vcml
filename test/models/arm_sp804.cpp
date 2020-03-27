/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

class sp804_stim: public test_base
{
public:
    master_socket OUT;

    sc_out<bool> RESET_OUT;

    sc_in<bool> IRQ1;
    sc_in<bool> IRQ2;
    sc_in<bool> IRQC;

    sp804_stim(const sc_module_name& nm):
        test_base(nm),
        OUT("OUT"),
        IRQ1("IRQ1"),
        IRQ2("IRQ2"),
        IRQC("IRQC") {
    }

    virtual void run_test() override {
        const u64 TIMER1_LOAD    = 0x00;
        const u64 TIMER1_VALUE   = 0x04;
        const u64 TIMER1_CONTROL = 0x08;
        u32 val;

        val = 0x100;
        EXPECT_OK(OUT.writew(TIMER1_LOAD, val)) << "cannot set counter";

        // force a non-zero test starting time
        wait(100, SC_MS);
        sc_time start = sc_time_stamp();

        // enable timer
        val = 0;
        EXPECT_OK(OUT.readw(TIMER1_VALUE, val));
        EXPECT_GE(val, 0x100) << "counter changed while disabled";

        val = arm::sp804timer::timer::CONTROL_ENABLED |
              arm::sp804timer::timer::CONTROL_IRQEN   |
              arm::sp804timer::timer::CONTROL_ONESHOT |
              arm::sp804timer::timer::CONTROL_32BIT;
        EXPECT_OK(OUT.writew(TIMER1_CONTROL, val)) << "cannot write CONTROL";

        wait(IRQC.value_changed_event());

        EXPECT_TRUE(IRQ1) << "irq1 did not fire";
        EXPECT_TRUE(IRQC) << "irqc did not propagate from irq1";
        EXPECT_FALSE(IRQ2) << "irq2 randomly fired";

        EXPECT_EQ(sc_time_stamp(), start + clock_cycles(0x100))
            << "interrupt did not fire at correct time";

        EXPECT_OK(OUT.readw(TIMER1_VALUE, val)) << "cannot read counter";
        EXPECT_EQ(val, 0) << "counter did not stop at zero";

        // check if resetting works
        EXPECT_OK(OUT.readw(TIMER1_CONTROL, val)) << "cannot read CONTROL";
        EXPECT_NE(val, 0x20) << "TIMER1_CONTROL changed randomly";

        RESET_OUT = true;
        wait(10, SC_MS);
        RESET_OUT = false;
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(RESET);

        val = 0;
        EXPECT_OK(OUT.readw(TIMER1_CONTROL, val)) << "cannot read CONTROL";
        EXPECT_EQ(val, 0x20) << "TIMER1_CONTROL did not reset";
    }
};

TEST(sp804timer, main) {
    sc_signal<bool> irq1("irq1");
    sc_signal<bool> irq2("irq2");
    sc_signal<bool> irqc("irqc");

    sc_signal<bool> rst("reset");
    sc_signal<clock_t> clk("clock");

    generic::clock sysclk("SYSCLK", 1 * MHz);
    sysclk.CLOCK.bind(clk);

    sp804_stim stim("STIM");
    arm::sp804timer sp804("SP804");

    stim.OUT.bind(sp804.IN);
    stim.RESET_OUT.bind(rst);

    stim.CLOCK.bind(clk);
    stim.RESET.bind(rst);

    stim.IRQ1.bind(irq1);
    stim.IRQ2.bind(irq2);
    stim.IRQC.bind(irqc);

    sp804.CLOCK.bind(clk);
    sp804.RESET.bind(rst);

    sp804.IRQ1.bind(irq1);
    sp804.IRQ2.bind(irq2);
    sp804.IRQC.bind(irqc);

    sc_core::sc_start();
}
