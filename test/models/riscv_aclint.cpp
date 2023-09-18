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

class aclint_test : public test_base
{
public:
    tlm_initiator_socket out_mtimer;
    tlm_initiator_socket out_mswi;
    tlm_initiator_socket out_sswi;

    riscv::aclint aclint;

    gpio_target_socket irq_mtimer0;
    gpio_target_socket irq_mtimer1;

    gpio_target_socket irq_msw0;
    gpio_target_socket irq_msw1;

    gpio_target_socket irq_ssw0;
    gpio_target_socket irq_ssw1;

    aclint_test(const sc_module_name& nm):
        test_base(nm),
        out_mtimer("out"),
        out_mswi("out_mswi"),
        out_sswi("out_sswi"),
        aclint("aclint"),
        irq_mtimer0("irq_mtimer0"),
        irq_mtimer1("irq_mtimer1"),
        irq_msw0("irq_msw0"),
        irq_msw1("irq_msw1"),
        irq_ssw0("irq_ssw0"),
        irq_ssw1("irq_ssw1") {
        rst.bind(aclint.rst);
        clk.bind(aclint.clk);

        out_mtimer.bind(aclint.mtimer);
        out_mswi.bind(aclint.mswi);
        out_sswi.bind(aclint.sswi);

        aclint.irq_mtimer[0].bind(irq_mtimer0);
        aclint.irq_mtimer[1].bind(irq_mtimer1);
        aclint.irq_mswi[0].bind(irq_msw0);
        aclint.irq_mswi[1].bind(irq_msw1);
        aclint.irq_sswi[0].bind(irq_ssw0);
        aclint.irq_sswi[1].bind(irq_ssw1);
    }

    void test_timer() {
        // test that time register counts correctly
        u64 mtime;
        ASSERT_OK(out_mtimer.readw(0x7ff8, mtime)) << "cannot read mtime";
        ASSERT_EQ(mtime, 0) << "mtime not reset";
        u64 cycles = 123;
        wait(clock_cycles(cycles));
        ASSERT_OK(out_mtimer.readw(0x7ff8, mtime)) << "cannot read mtime";
        ASSERT_EQ(mtime, cycles) << "mtime not counting";

        // trigger IRQ_TIMER0 at t + d and IRQ_TIMER1 at t + 2d
        u64 d = 456, mtimecmp0, mtimecmp1;
        ASSERT_OK(out_mtimer.writew(0, mtime + d)) << "cannot write mtimecmp0";
        ASSERT_OK(out_mtimer.writew(8, mtime + 2 * d))
            << "cannot write mtimecmp1";
        ASSERT_OK(out_mtimer.readw(0, mtimecmp0)) << "cannot read mtimecmp0";
        ASSERT_OK(out_mtimer.readw(8, mtimecmp1)) << "cannot read mtimecmp1";
        ASSERT_EQ(mtimecmp0, mtime + d) << "mtimecmp0 holds wrong value";
        ASSERT_EQ(mtimecmp1, mtime + 2 * d) << "mtimecmp1 holds wrong value";
        ASSERT_FALSE(irq_mtimer0.read()) << "IRQ_TIMER_0 triggered early";
        ASSERT_FALSE(irq_mtimer1.read()) << "IRQ_TIMER_1 triggered early";
        wait(clock_cycles(d));
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(irq_mtimer0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_FALSE(irq_mtimer1.read()) << "IRQ_TIMER_1 triggered early";
        wait(clock_cycles(d));
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(irq_mtimer0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_TRUE(irq_mtimer1.read()) << "IRQ_TIMER_1 not triggered";

        // clear IRQ_TIMER0 and IRQ_TIMER1
        ASSERT_OK(out_mtimer.writew(0, ~0ul)) << "cannot write mtimecmp0";
        ASSERT_OK(out_mtimer.writew(8, ~0ul)) << "cannot write mtimecmp1";
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(irq_mtimer0.read()) << "IRQ_TIMER_0 not cleared";
        ASSERT_FALSE(irq_mtimer1.read()) << "IRQ_TIMER_1 not cleared";

        // schedule IRQ_TIMER0/1 in the past
        ASSERT_OK(out_mtimer.readw(0x7ff8, mtime)) << "cannot read mtime";
        ASSERT_OK(out_mtimer.writew(0, mtime - 1)) << "cannot write mtimecmp0";
        ASSERT_OK(out_mtimer.writew(8, mtime - 1)) << "cannot write mtimecmp1";
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(irq_mtimer0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_TRUE(irq_mtimer1.read()) << "IRQ_TIMER_1 not triggered";
    }

    void test_swi(tlm_initiator_socket& out, gpio_target_socket& irq0,
                  gpio_target_socket& irq1) {
        // test software generated interrupts
        u32 msip = ~0u;
        ASSERT_OK(out.writew(0x4, msip)) << "cannot write msip1";
        wait(SC_ZERO_TIME);
        ASSERT_OK(out.readw(0x4, msip)) << "cannot read msip1";
        ASSERT_EQ(msip, 1u) << "msip0 holds illegal value";
        ASSERT_FALSE(irq0.read()) << "IRQ_TIMER_0 triggered";
        ASSERT_TRUE(irq1.read()) << "IRQ_TIMER_1 not triggered";
        ASSERT_OK(out.writew(0x4, 0u)) << "cannot write msip1";
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(irq1.read()) << "IRQ_TIMER_1 not cleared";
    }

    virtual void run_test() override {
        ASSERT_FALSE(irq_mtimer0.read()) << "IRQ_TIMER_0 not reset";
        ASSERT_FALSE(irq_mtimer1.read()) << "IRQ_TIMER_1 not reset";
        ASSERT_FALSE(irq_msw0.read()) << "IRQ_SW_0 not reset";
        ASSERT_FALSE(irq_msw1.read()) << "IRQ_SW_1 not reset";
        ASSERT_FALSE(irq_ssw0.read()) << "IRQ_SW_0 not reset";
        ASSERT_FALSE(irq_ssw1.read()) << "IRQ_SW_1 not reset";

        test_timer();
        wait(SC_ZERO_TIME);

        test_swi(out_mswi, irq_msw0, irq_msw1);
        wait(SC_ZERO_TIME);

        test_swi(out_sswi, irq_ssw0, irq_ssw1);
        wait(SC_ZERO_TIME);
    }
};

TEST(aclint, aclint) {
    aclint_test test("test");
    sc_core::sc_start();
}
