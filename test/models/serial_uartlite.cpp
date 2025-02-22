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

class uartlite_test : public test_base, public serial_host
{
public:
    queue<u8> rxdata;

    tlm_initiator_socket out;

    gpio_initiator_socket reset_out;

    gpio_target_socket irq_in;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    serial::uartlite uart;

    MOCK_METHOD(void, update_irq, (bool));

    virtual void gpio_notify(const gpio_target_socket& s,
                             bool state) override {
        if (&s == &irq_in)
            update_irq(state);
        else
            test_base::gpio_notify(s, state);
    }

    virtual void serial_receive(u8 data) override {
        log_debug("received 0x%02hhx from uart", data);
        rxdata.push(data);
    }

    enum status_bits : u32 {
        RX_FIFO_DATA = bit(0),
        RX_FIFO_FULL = bit(1),
        TX_FIFO_EMPTY = bit(2),
        TX_FIFO_FULL = bit(3),
        INTR_ENABLED = bit(4),
        OVERRUN_ERROR = bit(5),
        FRAME_ERROR = bit(6),
        PARITY_ERROR = bit(7),
    };

    enum control_bits : u32 {
        RST_TX_FIFO = bit(0),
        RST_RX_FIFO = bit(1),
        ENABLE_INTR = bit(4),
    };

    char read_rxfifo() {
        u32 val = -1;
        EXPECT_OK(out.readw(0x0, val));
        log_debug("popping 0x%02x from uart", val);
        return val;
    }

    void write_txfifo(char data) {
        log_debug("pushing 0x%02hhx to uart", data);
        ASSERT_OK(out.writew<u32>(0x4, data));
    }

    u32 read_status() {
        u32 val = -1;
        EXPECT_OK(out.readw(0x8, val));
        return val;
    }

    void write_control(u32 val) { ASSERT_OK(out.writew(0xc, val)); }

    void test_rx() {
        write_control(RST_TX_FIFO | RST_RX_FIFO);

        EXPECT_EQ(read_rxfifo(), 0) << "empty rx fifo should return zero";
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY) << "invalid reset status";

        write_control(ENABLE_INTR);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY | INTR_ENABLED)
            << "cannot enable interrupts";

        EXPECT_CALL(*this, update_irq(true));
        EXPECT_CALL(*this, update_irq(false));

        const char* msg = "12345678";
        ASSERT_EQ(strlen(msg), 8) << "test data not suitable for rx fifo size";
        serial_tx.send(msg[0]);

        EXPECT_EQ(read_status(), TX_FIFO_EMPTY | RX_FIFO_DATA | INTR_ENABLED)
            << "data did not arrive in rx fifo";

        EXPECT_CALL(*this, update_irq(true)).Times(0);
        EXPECT_CALL(*this, update_irq(false)).Times(0);

        for (int i = 1; i < 8; i++)
            serial_tx.send(msg[i]);

        EXPECT_EQ(read_status(),
                  TX_FIFO_EMPTY | RX_FIFO_DATA | RX_FIFO_FULL | INTR_ENABLED)
            << "rx fifo full not set";

        for (int i = 0; i < 8; i++) {
            EXPECT_EQ(read_rxfifo(), msg[i])
                << "wrong data received at position " << i;
        }

        EXPECT_EQ(read_status(), TX_FIFO_EMPTY | INTR_ENABLED)
            << "rx fifo full/data bits not reset";
        write_control(0);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY)
            << "interrupt did not get disabled";
    }

    void test_rx_overflow() {
        write_control(RST_TX_FIFO | RST_RX_FIFO);

        const char* msg = "123456789";
        EXPECT_GT(strlen(msg), 8) << "data not suitable for overflowing";

        EXPECT_CALL(*this, update_irq(true)).Times(0);
        EXPECT_CALL(*this, update_irq(false)).Times(0);

        for (size_t i = 0; i < strlen(msg); i++)
            serial_tx.send(msg[i]);

        EXPECT_EQ(read_status(), TX_FIFO_EMPTY | RX_FIFO_DATA | RX_FIFO_FULL)
            << "rx fifo full not set";

        for (size_t i = 0; i < strlen(msg); i++) {
            u32 expected = i < 8 ? msg[i] : 0;
            EXPECT_EQ(read_rxfifo(), expected)
                << "wrong data received at position " << i;
        }

        write_control(0);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY)
            << "interrupt did not get disabled";
    }

    void test_tx() {
        write_control(RST_TX_FIFO | RST_RX_FIFO);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY) << "invalid reset status";

        write_control(ENABLE_INTR);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY | INTR_ENABLED)
            << "cannot enable interrupts";

        EXPECT_CALL(*this, update_irq(true)).Times(1);
        EXPECT_CALL(*this, update_irq(false)).Times(1);

        const char* msg = "12345678";
        ASSERT_EQ(strlen(msg), 8) << "test data not suitable for rx fifo size";
        for (int i = 0; i < 8; i++)
            write_txfifo(msg[i]);

        EXPECT_EQ(read_status(), TX_FIFO_FULL | INTR_ENABLED)
            << "tx fifo full not set";

        wait(8 * uart.serial_tx.cycle());
        // wait an extra cycle for completion
        wait(uart.serial_tx.cycle());

        EXPECT_LE(rxdata.size(), 8) << "more data transmitted than expected";
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY | INTR_ENABLED)
            << "tx fifo still not emptied";
        for (int i = 0; i < 8; i++) {
            EXPECT_EQ(rxdata.front(), msg[i])
                << "wrong data received at position " << i;
            rxdata.pop();
        }

        write_control(0);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY)
            << "interrupt did not get disabled";
    }

    void test_tx_overflow() {
        write_control(RST_TX_FIFO | RST_RX_FIFO);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY) << "invalid reset status";

        EXPECT_CALL(*this, update_irq(true)).Times(0);
        EXPECT_CALL(*this, update_irq(false)).Times(0);

        const char* msg = "123456789";
        ASSERT_GT(strlen(msg), 8) << "test data not suitable for overflowing";
        for (size_t i = 0; i < strlen(msg); i++)
            write_txfifo(msg[i]);

        EXPECT_EQ(read_status(), TX_FIFO_FULL) << "tx fifo full not set";

        wait(8 * uart.serial_tx.cycle());
        wait(uart.serial_tx.cycle()); // wait one more cycle to complete

        EXPECT_EQ(read_status(), TX_FIFO_EMPTY) << "tx fifo still not emptied";
        EXPECT_LE(rxdata.size(), 8) << "more data transmitted than expected";
        for (int i = 0; i < 8; i++) {
            EXPECT_EQ(rxdata.front(), msg[i])
                << "wrong data received at position " << i;
            rxdata.pop();
        }

        write_control(0);
        EXPECT_EQ(read_status(), TX_FIFO_EMPTY)
            << "interrupt did not get disabled";
    }

    uartlite_test(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
        rxdata(),
        out("out"),
        reset_out("reset_out"),
        irq_in("irq_in"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx"),
        uart("uartlite") {
        out.bind(uart.in);
        uart.irq.bind(irq_in);
        reset_out.bind(uart.rst);
        reset_out.bind(rst);
        clk.bind(uart.clk);

        uart.serial_rx.bind(serial_tx);
        uart.serial_tx.bind(serial_rx);

        uart.reset();

        EXPECT_STREQ(uart.kind(), "vcml::serial::uartlite");

        add_test("test_rx", &uartlite_test::test_rx);
        add_test("test_rx_overflow", &uartlite_test::test_rx_overflow);
        add_test("test_tx", &uartlite_test::test_tx);
        add_test("test_tx_overflow", &uartlite_test::test_tx_overflow);
    }
};

TEST(serial, uartlite) {
    uartlite_test testbench("test");
    sc_core::sc_start();
}
