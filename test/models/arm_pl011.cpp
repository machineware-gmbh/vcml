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

class pl011_stim: public test_base
{
public:
    master_socket OUT;

    sc_out<bool> RESET_OUT;

    sc_in<bool> IRQ_IN;

    pl011_stim(const sc_module_name& nm):
        test_base(nm),
        OUT("OUT"),
        RESET_OUT("RESET_OUT"),
        IRQ_IN("IRQ_IN") {
    }

    virtual void run_test() override {
        u32 val;

        const u64 PL011_UARTDR   = 0x00;
        const u64 PL011_UARTFR   = 0x18;
        const u64 PL011_UARTCR   = 0x30;
        const u64 PL011_UARTIMSC = 0x38;
        const u64 PL011_UARTRIS  = 0x3c;

        // check initial UART status
        val = 0;
        EXPECT_OK(OUT.readw(PL011_UARTFR, val));
        EXPECT_TRUE(val & arm::pl011uart::FR_RXFE) << "RX FIFO not empty";
        EXPECT_TRUE(val & arm::pl011uart::FR_TXFE) << "TX FIFO not empty";

        // enable UART and transmission
        val = arm::pl011uart::CR_TXE | arm::pl011uart::CR_UARTEN;
        EXPECT_OK(OUT.writew(PL011_UARTCR, val)) << "cannot write UARTCR";

        // send data
        val = 'X';
        EXPECT_OK(OUT.writew(PL011_UARTDR, val)) << "cannot write UARTDR";
        wait(clock_cycle());

        // check raw interrupt status
        val = 0;
        EXPECT_OK(OUT.readw(PL011_UARTRIS, val)) << "cannot read UARTRIS";
        EXPECT_EQ(val, arm::pl011uart::RIS_TX) << "bogus irq state returned";
        wait(clock_cycle());
        EXPECT_FALSE(IRQ_IN) << "spurious interrupt received";

        // set interrupt mask, await interrupt
        val = arm::pl011uart::RIS_TX;
        EXPECT_OK(OUT.writew(PL011_UARTIMSC, val)) << "cannot write UARTIMSC";
        wait(IRQ_IN.value_changed_event());
        EXPECT_TRUE(IRQ_IN) << "interrupt did not trigger";

        // check reset works
        RESET_OUT = true;
        wait(10, SC_MS);
        RESET_OUT = false;
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(RESET);

        val = 0;
        EXPECT_OK(OUT.readw(PL011_UARTFR, val));
        EXPECT_TRUE(val & arm::pl011uart::FR_RXFE) << "RX FIFO not reset";
        EXPECT_TRUE(val & arm::pl011uart::FR_TXFE) << "TX FIFO not reset";
        EXPECT_FALSE(IRQ_IN) << "interrupt state did not reset";
    }

};

TEST(arm_pl011, main) {
    sc_signal<bool> irq("irq");
    sc_signal<bool> rst("reset");
    sc_signal<clock_t> clk("clock");

    generic::clock sysclk("SYSCLK", 1 * MHz);
    sysclk.CLOCK.bind(clk);

    pl011_stim stim("STIM");
    arm::pl011uart pl011("PL011");

    stim.OUT.bind(pl011.IN);
    stim.RESET_OUT.bind(rst);
    stim.CLOCK.bind(clk);
    stim.RESET.bind(rst);
    stim.IRQ_IN.bind(irq);

    pl011.CLOCK.bind(clk);
    pl011.RESET.bind(rst);
    pl011.IRQ.bind(irq);

    sc_core::sc_start();
}
