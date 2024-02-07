/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class sifive_bench : public test_base, public serial_host
{
public:
    tlm_initiator_socket out;

    gpio_initiator_socket reset_out;

    gpio_target_socket tx_irq_in;
    gpio_target_socket rx_irq_in;

    serial_initiator_socket bench_tx;
    serial_target_socket bench_rx;

    serial::sifive uart;

    MOCK_METHOD(void, serial_receive, (u8), (override));

    sifive_bench(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
        out("out"),
        reset_out("reset_out"),
        tx_irq_in("tx_irq_in"),
        rx_irq_in("rx_irq_in"),
        bench_tx("bench_tx"),
        bench_rx("bench_rx"),
        uart("sifive_uart") {
        out.bind(uart.in);
        uart.tx_irq.bind(tx_irq_in);
        uart.rx_irq.bind(rx_irq_in);
        reset_out.bind(uart.rst);
        reset_out.bind(rst);
        clk.bind(uart.clk);

        uart.serial_rx.bind(bench_tx);
        uart.serial_tx.bind(bench_rx);
    }

    virtual void run_test() override {
        enum addresses2 : u64 {
            TXDATA = 0x00,
            RXDATA = 0x04,
            TXCTRL = 0x08,
            RXCTRL = 0x0c,
            IE = 0x10,
            IP = 0x14,

        };
        u32 val = 0;
        // check initial UART status
        EXPECT_OK(out.readw(TXDATA, val));
        EXPECT_EQ(val, 0x0) << "TXDATA not initialized to zero";
        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_EQ(val, 0x1 << 31) << "RXDATA not initialized to zero";
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_EQ(val, 0x0) << "TXCTRL not initialized to zero";
        EXPECT_OK(out.readw(RXCTRL, val));
        EXPECT_EQ(val, 0x0) << "RXCTRL not initialized to zero";
        EXPECT_OK(out.readw(IE, val));
        EXPECT_EQ(val, 0x0) << "IE not initialized to zero";
        EXPECT_OK(out.readw(IP, val));
        EXPECT_EQ(val, 0x0) << "IP not initialized to zero";
        EXPECT_TRUE(uart.is_rx_empty()) << "RX FIFO not empty";
        // enable UART and transmission do reg writes/reads
        val = 0x1;
        EXPECT_OK(out.writew(TXCTRL, val));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_EQ(val, 0x1);

        val = 0x1;
        EXPECT_OK(out.writew(RXCTRL, val));
        EXPECT_OK(out.readw(RXCTRL, val));
        EXPECT_EQ(val, 0x1);

        // write to rxdata and expect nothing to happen
        u32 prev_val;
        EXPECT_OK(out.readw(RXDATA, prev_val));
        val = 'x';
        EXPECT_OK(out.writew(RXDATA, val));
        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_EQ(val, prev_val);

        // read to txdata
        EXPECT_OK(out.readw(TXDATA, val));
        // set watermark and trigger an irq
        val = 0x1;
        EXPECT_OK(out.writew(IE, val));
        EXPECT_OK(out.readw(IE, val));
        EXPECT_EQ(val, 0x1);

        val = 0x1 | (4 << 16);
        EXPECT_OK(out.writew(TXCTRL, val));
        val = 'x';
        EXPECT_CALL(*this, serial_receive('x'));
        EXPECT_OK(out.writew(TXDATA, val));
        EXPECT_FALSE(rx_irq_in) << "wrong interrupt triggered";
        EXPECT_TRUE(tx_irq_in) << "interrupt did not trigger";
        wait(1, SC_MS);

        val = 0b10;
        EXPECT_OK(out.writew(IE, val));
        EXPECT_OK(out.readw(IE, val));
        EXPECT_EQ(val, 0b10);
        bench_tx.send('x');
        EXPECT_FALSE(tx_irq_in) << "wrong interrupt triggered";
        EXPECT_TRUE(rx_irq_in) << "interrupt did not trigger";
        wait(1, SC_MS);
        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_EQ(val, 'x');

        // check reset works

        reset_out = true;
        wait(10, SC_MS);
        reset_out = false;
        wait(SC_ZERO_TIME);
        ASSERT_FALSE(rst);

        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_EQ(val, 0x0) << "TXCTRL not reset to zero";
        EXPECT_OK(out.readw(RXCTRL, val));
        EXPECT_EQ(val, 0x0) << "RXCTRL not reset to zero";
        EXPECT_OK(out.readw(IE, val));
        EXPECT_EQ(val, 0x0) << "IE not reset to zero";
        EXPECT_FALSE(tx_irq_in) << "interrupt state did not reset";
    }
};

TEST(sifive, main) {
    sifive_bench bench("bench");
    sc_core::sc_start();
}
