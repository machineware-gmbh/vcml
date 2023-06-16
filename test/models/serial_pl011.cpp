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

class pl011_bench : public test_base
{
public:
    tlm_initiator_socket out;

    gpio_initiator_socket reset_out;

    gpio_target_socket irq_in;

    serial::pl011 uart;

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
        EXPECT_TRUE(val & serial::pl011::FR_RXFE) << "RX FIFO not empty";
        EXPECT_TRUE(val & serial::pl011::FR_TXFE) << "TX FIFO not empty";

        // enable UART and transmission
        val = serial::pl011::CR_TXE | serial::pl011::CR_UARTEN;
        EXPECT_OK(out.writew(PL011_UARTCR, val)) << "cannot write UARTCR";

        // send data
        val = 'X';
        EXPECT_OK(out.writew(PL011_UARTDR, val)) << "cannot write UARTDR";
        wait(clock_cycle());

        // check raw interrupt status
        val = 0;
        EXPECT_OK(out.readw(PL011_UARTRIS, val)) << "cannot read UARTRIS";
        EXPECT_EQ(val, serial::pl011::RIS_TX) << "bogus irq state returned";
        EXPECT_FALSE(irq_in) << "spurious interrupt received";

        // set interrupt mask, await interrupt
        val = serial::pl011::RIS_TX;
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
        EXPECT_TRUE(val & serial::pl011::FR_RXFE) << "RX FIFO not reset";
        EXPECT_TRUE(val & serial::pl011::FR_TXFE) << "TX FIFO not reset";
        EXPECT_FALSE(irq_in) << "interrupt state did not reset";
    }
};

TEST(arm_pl011, main) {
    pl011_bench bench("bench");
    sc_core::sc_start();
}
