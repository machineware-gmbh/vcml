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

class pl011_bench : public test_base
{
public:
    tlm_initiator_socket out;

    rst_initiator_socket reset_out;

    irq_target_socket irq_in;

    arm::pl011uart uart;

    pl011_bench(const sc_module_name& nm):
        test_base(nm),
        out("out"),
        reset_out("reset_out"),
        irq_in("irq_in"),
        uart("pl011") {
        out.bind(uart.in);
        uart.irq.bind(irq_in);
        reset_out.bind(uart.rst);
        reset_out.bind(rst);
        clk.bind(uart.clk);

        uart.serial_rx.stub();
        uart.serial_tx.stub();
    }

    virtual void run_test() override {
        enum addresses : u64 {
            PL011_UARTDR = 0x00,
            PL011_UARTFR = 0x18,
            PL011_UARTCR = 0x30,
            PL011_UARTIMSC = 0x38,
            PL011_UARTRIS = 0x3c,
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
    pl011_bench bench("bench");
    sc_core::sc_start();
}
