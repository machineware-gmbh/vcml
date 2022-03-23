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

class pl011_stim : public test_base
{
public:
    tlm_initiator_socket out;

    sc_out<bool> reset_out;

    irq_target_socket irq_in;

    pl011_stim(const sc_module_name& nm):
        test_base(nm), out("out"), reset_out("reset_out"), irq_in("irq_in") {}

    virtual void run_test() override {
        enum addresses : u64 {
            PL011_UARTDR   = 0x00,
            PL011_UARTFR   = 0x18,
            PL011_UARTCR   = 0x30,
            PL011_UARTIMSC = 0x38,
            PL011_UARTRIS  = 0x3c,
        };

        // check initial UART status
        u32 val = 0;
        EXPECT_OK(out.readw(PL011_UARTFR, val));
        EXPECT_TRUE(val & arm::pl011uart::FR_RXFE) << "RX FIFO not empty";
        EXPECT_TRUE(val & arm::pl011uart::FR_TXFE) << "TX FIFO not empty";

        // enable UART and transmission
        val = arm::pl011uart::CR_TXE | arm::pl011uart::CR_UARTEN;
        EXPECT_OK(out.writew(PL011_UARTCR, val)) << "cannot write UARTCR";

        // send data
        val = 'X';
        EXPECT_OK(out.writew(PL011_UARTDR, val)) << "cannot write UARTDR";
        wait(clock_cycle());

        // check raw interrupt status
        val = 0;
        EXPECT_OK(out.readw(PL011_UARTRIS, val)) << "cannot read UARTRIS";
        EXPECT_EQ(val, arm::pl011uart::RIS_TX) << "bogus irq state returned";
        EXPECT_FALSE(irq_in) << "spurious interrupt received";

        // set interrupt mask, await interrupt
        val = arm::pl011uart::RIS_TX;
        EXPECT_OK(out.writew(PL011_UARTIMSC, val)) << "cannot write UARTIMSC";
        EXPECT_TRUE(irq_in) << "interrupt did not trigger";

        // check reset works
        reset_out = true;
        wait(10, SC_MS);
        reset_out = false;
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(rst);

        val = 0;
        EXPECT_OK(out.readw(PL011_UARTFR, val));
        EXPECT_TRUE(val & arm::pl011uart::FR_RXFE) << "RX FIFO not reset";
        EXPECT_TRUE(val & arm::pl011uart::FR_TXFE) << "TX FIFO not reset";
        EXPECT_FALSE(irq_in) << "interrupt state did not reset";
    }
};

TEST(arm_pl011, main) {
    sc_signal<bool> rst("reset");
    sc_signal<clock_t> clk("clock");

    generic::clock sysclk("sysclk", 1 * MHz);
    sysclk.clk.bind(clk);

    pl011_stim stim("stim");
    arm::pl011uart pl011("pl011");

    stim.out.bind(pl011.in);
    stim.reset_out.bind(rst);
    stim.clk.bind(clk);
    stim.rst.bind(rst);

    pl011.clk.bind(clk);
    pl011.rst.bind(rst);
    pl011.irq.bind(stim.irq_in);

    sc_core::sc_start();
}
