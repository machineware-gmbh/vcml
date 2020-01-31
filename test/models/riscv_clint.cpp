/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "vcml.h"

using namespace ::testing;
using namespace ::sc_core;
using namespace ::vcml;

#define ASSERT_OK(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_OK_RESPONSE)
#define ASSERT_AE(tlmcall) ASSERT_EQ(tlmcall, tlm::TLM_ADDRESS_ERROR_RESPONSE)

class mock_stim: public vcml::component
{
private:
    SC_HAS_PROCESS(mock_stim);

    void run_test() {
        // test that interrupts are reset
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(IRQ_SW_0.read()) << "IRQ_SW_0 not reset";
        ASSERT_FALSE(IRQ_SW_1.read()) << "IRQ_SW_1 not reset";
        ASSERT_FALSE(IRQ_TIMER_0.read()) << "IRQ_TIMER_0 not reset";
        ASSERT_FALSE(IRQ_TIMER_1.read()) << "IRQ_TIMER_1 not reset";

        // test that time register counts correctly
        u64 mtime;
        ASSERT_OK(OUT.readw(0xbff8, mtime)) << "cannot read mtime";
        ASSERT_EQ(mtime, 0) << "mtime not reset";
        u64 cycles = 123;
        wait(clock_cycles(cycles));
        ASSERT_OK(OUT.readw(0xbff8, mtime)) << "cannot read mtime";
        ASSERT_EQ(mtime, cycles) << "mtime not counting";

        // trigger IRQ_TIMER0 at t + d and IRQ_TIMER1 at t + 2d
        u64 d = 456, mtimecmp0, mtimecmp1;
        ASSERT_OK(OUT.writew(0x4000, mtime + d)) << "cannot write mtimecmp0";
        ASSERT_OK(OUT.writew(0x4008, mtime + 2*d)) << "cannot write mtimecmp1";
        ASSERT_OK(OUT.readw(0x4000, mtimecmp0)) << "cannot read mtimecmp0";
        ASSERT_OK(OUT.readw(0x4008, mtimecmp1)) << "cannot read mtimecmp1";
        ASSERT_EQ(mtimecmp0, mtime + d) << "mtimecmp0 holds wrong value";
        ASSERT_EQ(mtimecmp1, mtime + 2*d) << "mtimecmp1 holds wrong value";
        ASSERT_FALSE(IRQ_TIMER_0.read()) << "IRQ_TIMER_0 triggered early";
        ASSERT_FALSE(IRQ_TIMER_1.read()) << "IRQ_TIMER_1 triggered early";
        wait(clock_cycles(d));
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(IRQ_TIMER_0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_FALSE(IRQ_TIMER_1.read()) << "IRQ_TIMER_1 triggered early";
        wait(clock_cycles(d));
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(IRQ_TIMER_0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_TRUE(IRQ_TIMER_1.read()) << "IRQ_TIMER_1 not triggered";

        // clear IRQ_TIMER0 and IRQ_TIMER1
        ASSERT_OK(OUT.writew(0x4000, ~0ul)) << "cannot write mtimecmp0";
        ASSERT_OK(OUT.writew(0x4008, ~0ul)) << "cannot write mtimecmp1";
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(IRQ_TIMER_0.read()) << "IRQ_TIMER_0 not cleared";
        ASSERT_FALSE(IRQ_TIMER_1.read()) << "IRQ_TIMER_1 not cleared";

        // schedule IRQ_TIMER0/1 in the past
        ASSERT_OK(OUT.readw(0xbff8, mtime)) << "cannot read mtime";
        ASSERT_OK(OUT.writew(0x4000, mtime - 1)) << "cannot write mtimecmp0";
        ASSERT_OK(OUT.writew(0x4008, mtime - 1)) << "cannot write mtimecmp1";
        wait(SC_ZERO_TIME);
        ASSERT_TRUE(IRQ_TIMER_0.read()) << "IRQ_TIMER_0 not triggered";
        ASSERT_TRUE(IRQ_TIMER_1.read()) << "IRQ_TIMER_1 not triggered";

        // test software generated interrupts
        u32 msip = ~0u;
        ASSERT_OK(OUT.writew(0x4, msip)) << "cannot write msip1";
        wait(SC_ZERO_TIME);
        ASSERT_OK(OUT.readw(0x4, msip)) << "cannot read msip1";
        ASSERT_EQ(msip, 1u) << "msip0 holds illegal value";
        ASSERT_FALSE(IRQ_SW_0.read()) << "IRQ_TIMER_0 triggered";
        ASSERT_TRUE(IRQ_SW_1.read()) << "IRQ_TIMER_1 not triggered";
        ASSERT_OK(OUT.writew(0x4, 0u)) << "cannot write msip1";
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(IRQ_SW_1.read()) << "IRQ_TIMER_1 not cleared";
    }

    void run() {
        run_test();
        sc_core::sc_stop();
    }

public:
    vcml::master_socket OUT;

    sc_in<bool> IRQ_SW_0;
    sc_in<bool> IRQ_SW_1;

    sc_in<bool> IRQ_TIMER_0;
    sc_in<bool> IRQ_TIMER_1;

    mock_stim(const sc_core::sc_module_name& nm):
        component(nm),
        OUT("OUT"),
        IRQ_SW_0("IRQT1"),
        IRQ_SW_1("IRQT2"),
        IRQ_TIMER_0("IRQS1"),
        IRQ_TIMER_1("IRQS2") {
        SC_THREAD(run);
    }
};

TEST(clint, clint) {
    sc_signal<bool> irq_sw_0("irq_sw_0");
    sc_signal<bool> irq_sw_1("irq_sw_1");

    sc_signal<bool> irq_timer_0("irq_timer_0");
    sc_signal<bool> irq_timer_1("irq_timer_1");

    sc_signal<clock_t> clk_100mhz("clk_100mhz");

    mock_stim stim("STIM");
    riscv::clint clint("CLINT");
    generic::clock sysclk("SYSCLK", 100 * MHz);


    stim.RESET.stub();
    clint.RESET.stub();

    stim.CLOCK.bind(clk_100mhz);
    clint.CLOCK.bind(clk_100mhz);
    sysclk.CLOCK.bind(clk_100mhz);

    stim.OUT.bind(clint.IN);

    clint.IRQ_SW[0].bind(irq_sw_0);
    clint.IRQ_SW[1].bind(irq_sw_1);

    clint.IRQ_TIMER[0].bind(irq_timer_0);
    clint.IRQ_TIMER[1].bind(irq_timer_1);

    stim.IRQ_SW_0.bind(irq_sw_0);
    stim.IRQ_SW_1.bind(irq_sw_1);

    stim.IRQ_TIMER_0.bind(irq_timer_0);
    stim.IRQ_TIMER_1.bind(irq_timer_1);

    sc_core::sc_start();
}
