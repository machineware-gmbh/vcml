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

class clint_stim : public test_base
{
public:
    tlm_initiator_socket out;

    gpio_target_socket irq_sw_0;
    gpio_target_socket irq_sw_1;

    gpio_target_socket irq_timer_0;
    gpio_target_socket irq_timer_1;

    clint_stim(const sc_module_name& nm):
        test_base(nm),
        out("out"),
        irq_sw_0("irq_sw_0"),
        irq_sw_1("irq_sw_1"),
        irq_timer_0("irq_timer_0"),
        irq_timer_1("irq_timer_1") {}

    virtual void run_test() override {
        ASSERT_FALSE(irq_sw_0.read()) << "IRQ_SW_0 not reset";
        ASSERT_FALSE(irq_sw_1.read()) << "IRQ_SW_1 not reset";
        ASSERT_FALSE(irq_timer_0.read()) << "IRQ_TIMER_0 not reset";
        ASSERT_FALSE(irq_timer_1.read()) << "IRQ_TIMER_1 not reset";

        // test that time register counts correctly
        u64 mtime;
        ASSERT_OK(out.readw(0xbff8, mtime)) << "cannot read mtime";
        ASSERT_EQ(mtime, 0) << "mtime not reset";
        u64 cycles = 123;
        wait(clock_cycles(cycles));
        ASSERT_OK(out.readw(0xbff8, mtime)) << "cannot read mtime";
        ASSERT_EQ(mtime, cycles) << "mtime not counting";

        // trigger IRQ_TIMER0 at t + d and IRQ_TIMER1 at t + 2d
        u64 d = 456, mtimecmp0, mtimecmp1;
        ASSERT_OK(out.writew(0x4000, mtime + d)) << "cannot write mtimecmp0";
        ASSERT_OK(out.writew(0x4008, mtime + 2 * d))
            << "cannot write mtimecmp1";
        ASSERT_OK(out.readw(0x4000, mtimecmp0)) << "cannot read mtimecmp0";
        ASSERT_OK(out.readw(0x4008, mtimecmp1)) << "cannot read mtimecmp1";
        ASSERT_EQ(mtimecmp0, mtime + d) << "mtimecmp0 holds wrong value";
        ASSERT_EQ(mtimecmp1, mtime + 2 * d) << "mtimecmp1 holds wrong value";
        ASSERT_FALSE(irq_timer_0.read()) << "IRQ_TIMER_0 triggered early";
        ASSERT_FALSE(irq_timer_1.read()) << "IRQ_TIMER_1 triggered early";
        wait(clock_cycles(d));
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(irq_timer_0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_FALSE(irq_timer_1.read()) << "IRQ_TIMER_1 triggered early";
        wait(clock_cycles(d));
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(irq_timer_0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_TRUE(irq_timer_1.read()) << "IRQ_TIMER_1 not triggered";

        // clear IRQ_TIMER0 and IRQ_TIMER1
        ASSERT_OK(out.writew(0x4000, ~0ul)) << "cannot write mtimecmp0";
        ASSERT_OK(out.writew(0x4008, ~0ul)) << "cannot write mtimecmp1";
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(irq_timer_0.read()) << "IRQ_TIMER_0 not cleared";
        ASSERT_FALSE(irq_timer_1.read()) << "IRQ_TIMER_1 not cleared";

        // schedule IRQ_TIMER0/1 in the past
        ASSERT_OK(out.readw(0xbff8, mtime)) << "cannot read mtime";
        ASSERT_OK(out.writew(0x4000, mtime - 1)) << "cannot write mtimecmp0";
        ASSERT_OK(out.writew(0x4008, mtime - 1)) << "cannot write mtimecmp1";
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(irq_timer_0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_TRUE(irq_timer_1.read()) << "IRQ_TIMER_1 not triggered";

        // test software generated interrupts
        u32 msip = ~0u;
        ASSERT_OK(out.writew(0x4, msip)) << "cannot write msip1";
        wait(SC_ZERO_TIME);
        ASSERT_OK(out.readw(0x4, msip)) << "cannot read msip1";
        ASSERT_EQ(msip, 1u) << "msip0 holds illegal value";
        ASSERT_FALSE(irq_sw_0.read()) << "IRQ_TIMER_0 triggered";
        ASSERT_TRUE(irq_sw_1.read()) << "IRQ_TIMER_1 not triggered";
        ASSERT_OK(out.writew(0x4, 0u)) << "cannot write msip1";
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(irq_sw_1.read()) << "IRQ_TIMER_1 not cleared";
    }
};

TEST(clint, clint) {
    clint_stim stim("stim");
    riscv::clint clint("clint");

    stim.clk.bind(clint.clk);
    stim.rst.bind(clint.rst);
    stim.out.bind(clint.in);

    clint.irq_sw[0].bind(stim.irq_sw_0);
    clint.irq_sw[1].bind(stim.irq_sw_1);

    clint.irq_timer[0].bind(stim.irq_timer_0);
    clint.irq_timer[1].bind(stim.irq_timer_1);

    sc_core::sc_start();
}
