/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class sifive_uart_bench : public test_base, public serial_host
{
public:
    tlm_initiator_socket out;

    gpio_initiator_socket reset_out;

    gpio_target_socket irq_in;

    serial::sifive_uart uart;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    sifive_uart_bench(const sc_module_name& nm):
        test_base(nm),
        out("out"),
        reset_out("reset_out"),
        irq_in("irq_in"),
        uart("sifive_uart"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx") {
        out.bind(uart.in);
        uart.irq.bind(irq_in);
        reset_out.bind(uart.rst);
        reset_out.bind(rst);
        clk.bind(uart.clk);

        uart.serial_rx.bind(serial_tx);
        uart.serial_tx.bind(serial_rx);
    }

    MOCK_METHOD(void, serial_receive, (u8), (override));

    virtual void run_test() override {
        static const u32 SIFIVE_UART_TXDATA_FULL   = 1U << 31U;
        static const u32 SIFIVE_UART_RXDATA_EMPTY  = 1U << 31U;
        static const u32 SIFIVE_UART_TXCTRL_TXEN   = 1U;
        static const u32 SIFIVE_UART_TXCTRL_NSTOP  = 1U << 1U;
        static const u32 SIFIVE_UART_RXCTRL_RXEN   = 1U;
        static const u32 SIFIVE_UART_WM_OFFSET     = 16U;

        static const u32 SIFIVE_UART_IE_TXWM       = 1; /* Transmit watermark interrupt enable */
        static const u32 SIFIVE_UART_IE_RXWM       = 2; /* Receive watermark interrupt enable */

        static const u32 SIFIVE_UART_IP_TXWM       = 1; /* Transmit watermark interrupt pending */
        static const u32 SIFIVE_UART_IP_RXWM       = 2; /* Receive watermark interrupt pending */

        enum addresses : u64 {
            TXDATA = 0x00,
            RXDATA = 0x04,
            TXCTRL = 0x08,
            RXCTRL = 0x0c,
            IE     = 0x10,
            IP     = 0x14,
            DIV    = 0x18
        };

        u32 val = 0;

        // check initial UART status
        EXPECT_OK(out.readw(TXDATA, val));
        EXPECT_TRUE(val == 0);

        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_TRUE(val == SIFIVE_UART_RXDATA_EMPTY);

        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == 0);

        EXPECT_OK(out.readw(RXCTRL, val));
        EXPECT_TRUE(val == 0);

        EXPECT_OK(out.readw(IE, val));
        EXPECT_TRUE(val == 0);

        EXPECT_OK(out.readw(IP, val));
        EXPECT_TRUE(val == 0);

        EXPECT_OK(out.readw(DIV, val));
        EXPECT_TRUE(val == 0);

        // test reset
        val = SIFIVE_UART_RXCTRL_RXEN;
        EXPECT_OK(out.writew(RXCTRL, val));

        val = 'O';
        EXPECT_OK(out.writew(TXDATA, val));
        serial_tx.send('Y');

        reset_out = true;
        reset_out = false;

        EXPECT_OK(out.readw(TXDATA, val));
        EXPECT_TRUE(val == 0);

        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_TRUE(val == SIFIVE_UART_RXDATA_EMPTY);

        EXPECT_OK(out.readw(DIV, val));
        EXPECT_TRUE(val == (u32)(uart.clock_hz() / SERIAL_115200BD));

        // test div
        val = 0xabcd;
        EXPECT_OK(out.writew(DIV, val));
        EXPECT_OK(out.readw(DIV, val));
        EXPECT_TRUE(val == 0xabcd);

        reset_out = true;
        reset_out = false;

        EXPECT_OK(out.readw(DIV, val));
        EXPECT_TRUE(val == (u32)(uart.clock_hz() / SERIAL_115200BD));

        // test tx
        val = SIFIVE_UART_TXCTRL_NSTOP; 
        EXPECT_OK(out.writew(TXCTRL, val));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == SIFIVE_UART_TXCTRL_NSTOP);
        EXPECT_OK(out.writew(TXCTRL, 0));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == 0);

        val = 'O';
        EXPECT_OK(out.writew(TXDATA, val));

        EXPECT_CALL(*this, serial_receive('O'));
        val = SIFIVE_UART_TXCTRL_TXEN; 
        EXPECT_OK(out.writew(TXCTRL, val));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == SIFIVE_UART_TXCTRL_TXEN);

        EXPECT_CALL(*this, serial_receive('X'));
        EXPECT_OK(out.writew(TXDATA, 'X'));

        val = 0;
        EXPECT_OK(out.writew(TXCTRL, val));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == 0);

        for (u64 i = 0; i < uart.tx_fifo_size; i++) {
            val = 'O';
            EXPECT_OK(out.writew(TXDATA, val));       
        }
        EXPECT_OK(out.readw(TXDATA, val));
        EXPECT_TRUE(val == SIFIVE_UART_TXDATA_FULL);

        EXPECT_CALL(*this, serial_receive('O')).Times(8);

        val = SIFIVE_UART_TXCTRL_TXEN; 
        EXPECT_OK(out.writew(TXCTRL, val));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == SIFIVE_UART_TXCTRL_TXEN);

        val = 0;
        EXPECT_OK(out.writew(TXCTRL, val));
        EXPECT_OK(out.readw(TXCTRL, val));
        EXPECT_TRUE(val == 0);

        // test rx
        serial_tx.send('Y');

        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_TRUE(val == SIFIVE_UART_RXDATA_EMPTY);

        val = SIFIVE_UART_RXCTRL_RXEN;
        EXPECT_OK(out.writew(RXCTRL, val));

        serial_tx.send('Y');

        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_TRUE(val == 'Y');

        for (u64 i = 0;i < uart.rx_fifo_size + 1; i++) {
            serial_tx.send('X');
        }

        for (u64 i = 0; i < uart.rx_fifo_size; i++) {
            EXPECT_OK(out.readw(RXDATA, val));
            EXPECT_TRUE(val == 'X');
        }

        EXPECT_OK(out.readw(RXDATA, val));
        EXPECT_TRUE(val == SIFIVE_UART_RXDATA_EMPTY);

        // test wathermark and interrupt setting
        val = SIFIVE_UART_IE_TXWM | SIFIVE_UART_IE_RXWM;
        EXPECT_OK(out.writew(IE, val));
        EXPECT_OK(out.readw(IE, val));
        EXPECT_TRUE(val == (SIFIVE_UART_IE_TXWM | SIFIVE_UART_IE_RXWM));

        EXPECT_FALSE(irq_in);

        val = 4 << SIFIVE_UART_WM_OFFSET;
        EXPECT_OK(out.writew(TXCTRL, val));

        EXPECT_TRUE(irq_in);
        
        EXPECT_OK(out.readw(IP, val));
        EXPECT_TRUE(val == SIFIVE_UART_IP_TXWM);

        serial_tx.send('X');
        EXPECT_OK(out.readw(IP, val));
        EXPECT_TRUE(val == (SIFIVE_UART_IP_TXWM | SIFIVE_UART_IP_RXWM));
    }
};

TEST(sifive_uart, main) {
    sifive_uart_bench bench("bench");
    sc_core::sc_start();
}
