/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : u64 {
    UART_THR = 0,
    UART_IER = 1,
    UART_IIR = 2,
    UART_FCR = 2,
    UART_LCR = 3,
    UART_MCR = 4,
    UART_LSR = 5,
    UART_MSR = 6,
    UART_SCR = 7,
    UART8250_BASE = 0x100,
};

enum : u8 {
    LSR_DR = serial::uart::LSR_DR,
    LSR_OE = serial::uart::LSR_OE,
    LSR_THRE = serial::uart::LSR_THRE,
    LSR_TEMT = serial::uart::LSR_TEMT,
    IRQ_RDA = serial::uart::IRQ_RDA,
    IRQ_THRE = serial::uart::IRQ_THRE,
    IRQ_RLS = serial::uart::IRQ_RLS,
    IIR_NOIP = serial::uart::IIR_NOIP,
    IIR_THRE = serial::uart::IIR_THRE,
    IIR_RDA = serial::uart::IIR_RDA,
    IIR_RLS = serial::uart::IIR_RLS,
    LCR_WL5 = serial::uart::LCR_WL5,
    LCR_WL8 = serial::uart::LCR_WL8,
    LCR_STP = serial::uart::LCR_STP,
    LCR_PEN = serial::uart::LCR_PEN,
    LCR_EPS = serial::uart::LCR_EPS,
    LCR_SPB = serial::uart::LCR_SPB,
    LCR_DLAB = serial::uart::LCR_DLAB,
    FCR_FE = serial::uart::FCR_FE,
    FCR_CRF = serial::uart::FCR_CRF,
    FCR_CTF = serial::uart::FCR_CTF,
    FCR_DMA = serial::uart::FCR_DMA,
};

class uart_test : public test_base, public serial_host
{
public:
    serial::uart16550 uart;
    serial::uart8250 uart8250;
    generic::bus bus;

    tlm_initiator_socket out;
    serial_initiator_socket serial_tx;
    serial_initiator_socket serial_tx8250;
    serial_target_socket serial_rx;
    serial_target_socket serial_rx8250;
    gpio_target_socket irq;
    gpio_target_socket irq8250;

    vector<serial_payload> sent;

    virtual void serial_receive(const serial_target_socket& socket,
                                serial_payload& tx) override {
        if (&socket == &serial_rx)
            sent.push_back(tx);
    }

    uart_test(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
        uart("uart"),
        uart8250("uart8250"),
        bus("bus"),
        out("out"),
        serial_tx("serial_tx"),
        serial_tx8250("serial_tx8250"),
        serial_rx("serial_rx"),
        serial_rx8250("serial_rx8250"),
        irq("irq"),
        irq8250("irq8250"),
        sent() {
        bus.bind(out);
        bus.bind(uart.in, 0, 7);
        bus.bind(uart8250.in, UART8250_BASE, UART8250_BASE + 7);
        uart.serial_tx.bind(serial_rx);
        uart8250.serial_tx.bind(serial_rx8250);
        serial_tx.bind(uart.serial_rx);
        serial_tx8250.bind(uart8250.serial_rx);
        uart.irq.bind(irq);
        uart8250.irq.bind(irq8250);

        for (component* comp :
             std::initializer_list<component*>{ &uart, &uart8250, &bus }) {
            rst.bind(comp->rst);
            clk.bind(comp->clk);
        }

        add_test("strings", &uart_test::test_strings);
        add_test("defaults", &uart_test::test_defaults);
        add_test("transmit", &uart_test::test_transmit);
        add_test("receive", &uart_test::test_receive);
        add_test("overrun", &uart_test::test_overrun);
        add_test("fifo_control", &uart_test::test_fifo_control);
        add_test("divisor", &uart_test::test_divisor);
        add_test("line_control", &uart_test::test_line_control);
        add_test("registers", &uart_test::test_registers);
    }

    u8 read(u64 addr) {
        u8 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u8 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    serial_payload transmit(u8 data) {
        sent.clear();
        write(UART_THR, data);
        EXPECT_EQ(sent.size(), 1);
        return sent.empty() ? serial_payload() : sent[0];
    }

    void test_strings() {
        EXPECT_STREQ(uart.kind(), "vcml::serial::uart16550");
        EXPECT_STREQ(uart.serial::uart::kind(), "vcml::serial::uart");
        EXPECT_STREQ(uart.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(uart8250.kind(), "vcml::serial::uart8250");
        EXPECT_STREQ(uart8250.version(), VCML_VERSION_STRING);
    }

    void test_defaults() {
        for (u64 base : { (u64)0, (u64)UART8250_BASE }) {
            EXPECT_EQ(read(base + UART_IER), 0);
            EXPECT_EQ(read(base + UART_IIR), IIR_NOIP);
            EXPECT_EQ(read(base + UART_LCR), LCR_WL8);
            EXPECT_EQ(read(base + UART_MCR), 0);
            EXPECT_EQ(read(base + UART_LSR), LSR_THRE | LSR_TEMT);
            EXPECT_EQ(read(base + UART_MSR), 0);
            EXPECT_EQ(read(base + UART_SCR), 0);
            EXPECT_EQ(read(base + UART_THR), 0);
        }

        EXPECT_FALSE(irq);
        EXPECT_FALSE(irq8250);

        u8 data = 0;
        EXPECT_CE(out.writew(UART_LSR, data));
    }

    void test_transmit() {
        serial_payload tx = transmit('A');
        EXPECT_EQ(tx.data, 'A');
        EXPECT_EQ(tx.baud, serial::uart::DEFAULT_BAUD);
        EXPECT_EQ(tx.width, SERIAL_8_BITS);
        EXPECT_EQ(tx.parity, SERIAL_PARITY_NONE);
        EXPECT_EQ(tx.stop, SERIAL_STOP_1);
        EXPECT_EQ(read(UART_LSR), LSR_THRE | LSR_TEMT);
        EXPECT_FALSE(irq);
        EXPECT_EQ(read(UART_IIR), IIR_THRE);
        EXPECT_EQ(read(UART_IIR), IIR_NOIP);

        write(UART_IER, IRQ_THRE);
        EXPECT_EQ(read(UART_IER), IRQ_THRE);
        EXPECT_FALSE(irq);
        EXPECT_EQ(transmit('B').data, 'B');
        EXPECT_TRUE(irq);
        EXPECT_EQ(read(UART_IIR), IIR_THRE);
        EXPECT_FALSE(irq);
        EXPECT_EQ(read(UART_IIR), IIR_NOIP);

        write(UART_IER, 0xff);
        EXPECT_EQ(read(UART_IER), 0x0f);
        write(UART_IER, 0);
    }

    void test_receive() {
        serial_tx.send('x');
        EXPECT_EQ(read(UART_LSR), LSR_DR | LSR_THRE | LSR_TEMT);
        EXPECT_EQ(read(UART_IIR), IIR_RDA);
        EXPECT_FALSE(irq);

        write(UART_IER, IRQ_RDA);
        EXPECT_TRUE(irq);
        serial_tx.send('y');
        EXPECT_EQ(read(UART_THR), 'x');
        EXPECT_TRUE(irq);
        EXPECT_EQ(read(UART_THR), 'y');
        EXPECT_FALSE(irq);
        EXPECT_EQ(read(UART_LSR), LSR_THRE | LSR_TEMT);
        EXPECT_EQ(read(UART_IIR), IIR_NOIP);
        EXPECT_EQ(read(UART_THR), 0);

        write(UART_IER, 0);
        EXPECT_FALSE(irq8250);
    }

    void test_overrun() {
        for (size_t i = 0; i < serial::uart16550::RX_FIFO_SIZE; i++)
            serial_tx.send('a' + i);
        EXPECT_EQ(read(UART_LSR), LSR_DR | LSR_THRE | LSR_TEMT);

        write(UART_IER, IRQ_RLS);
        EXPECT_FALSE(irq);
        serial_tx.send('!');
        EXPECT_TRUE(irq);
        EXPECT_EQ(read(UART_IIR), IIR_RLS);
        EXPECT_EQ(read(UART_LSR), LSR_OE | LSR_DR | LSR_THRE | LSR_TEMT);
        EXPECT_FALSE(irq);
        EXPECT_EQ(read(UART_LSR), LSR_DR | LSR_THRE | LSR_TEMT);

        for (size_t i = 0; i < serial::uart16550::RX_FIFO_SIZE; i++)
            EXPECT_EQ(read(UART_THR), 'a' + i);
        EXPECT_EQ(read(UART_LSR), LSR_THRE | LSR_TEMT);
        write(UART_IER, 0);

        serial_tx8250.send('1');
        EXPECT_EQ(read(UART8250_BASE + UART_LSR),
                  LSR_DR | LSR_THRE | LSR_TEMT);
        serial_tx8250.send('2');
        EXPECT_EQ(read(UART8250_BASE + UART_LSR),
                  LSR_OE | LSR_DR | LSR_THRE | LSR_TEMT);
        EXPECT_EQ(read(UART8250_BASE + UART_THR), '1');
        EXPECT_EQ(read(UART8250_BASE + UART_LSR), LSR_THRE | LSR_TEMT);
    }

    void test_fifo_control() {
        write(UART_IER, IRQ_RDA);
        serial_tx.send('a');
        serial_tx.send('b');
        EXPECT_TRUE(irq);

        write(UART_FCR, FCR_FE | FCR_CRF);
        EXPECT_EQ(read(UART_LSR), LSR_THRE | LSR_TEMT);
        EXPECT_FALSE(irq);
        EXPECT_EQ(read(UART_THR), 0);

        for (u8 trigger : { 0x00, 0x40, 0x80, 0xc0 })
            write(UART_FCR, trigger | FCR_FE | FCR_CTF | FCR_DMA);
        EXPECT_EQ(read(UART_LSR), LSR_THRE | LSR_TEMT);

        serial_tx.send('c');
        EXPECT_EQ(read(UART_THR), 'c');
        write(UART_FCR, 0);
        write(UART_IER, 0);
    }

    void test_divisor() {
        const u16 divisor = clk.get_hz() / (16 * serial::uart::DEFAULT_BAUD);
        write(UART_LCR, LCR_WL8 | LCR_DLAB);
        EXPECT_EQ(read(UART_THR), divisor & 0xff);
        EXPECT_EQ(read(UART_IER), divisor >> 8);

        write(UART_THR, 54);
        write(UART_IER, 0);
        EXPECT_EQ(read(UART_THR), 54);
        EXPECT_EQ(read(UART_IER), 0);
        write(UART_LCR, LCR_WL8);
        EXPECT_EQ(transmit('C').baud, clk.get_hz() / (54 * 16));

        write(UART_LCR, LCR_WL8 | LCR_DLAB);
        write(UART_THR, 0);
        EXPECT_EQ(read(UART_THR), divisor & 0xff);
        EXPECT_EQ(read(UART_IER), divisor >> 8);
        write(UART_LCR, LCR_WL8);
        EXPECT_EQ(transmit('D').baud, serial::uart::DEFAULT_BAUD);
    }

    void test_line_control() {
        for (u8 wl = 0; wl < 4; wl++) {
            write(UART_LCR, wl);
            EXPECT_EQ(read(UART_LCR), wl);
            serial_payload tx = transmit(0xff);
            EXPECT_EQ(tx.width, 5 + wl);
            EXPECT_EQ(tx.data & tx.mask, bitmask(5 + wl));
            EXPECT_EQ(tx.stop, SERIAL_STOP_1);
            EXPECT_EQ(tx.parity, SERIAL_PARITY_NONE);
        }

        write(UART_LCR, LCR_WL8 | LCR_STP);
        EXPECT_EQ(transmit('E').stop, SERIAL_STOP_2);
        write(UART_LCR, LCR_WL5 | LCR_STP);
        EXPECT_EQ(transmit('E').stop, SERIAL_STOP_1_5);

        write(UART_LCR, LCR_WL8 | LCR_PEN);
        EXPECT_EQ(transmit('F').parity, SERIAL_PARITY_ODD);
        EXPECT_EQ(transmit('F').stop, SERIAL_STOP_1);
        write(UART_LCR, LCR_WL8 | LCR_PEN | LCR_EPS);
        EXPECT_EQ(transmit('F').parity, SERIAL_PARITY_EVEN);
        write(UART_LCR, LCR_WL8 | LCR_PEN | LCR_SPB);
        EXPECT_EQ(transmit('F').parity, SERIAL_PARITY_MARK);
        write(UART_LCR, LCR_WL8 | LCR_PEN | LCR_EPS | LCR_SPB);
        EXPECT_EQ(transmit('F').parity, SERIAL_PARITY_SPACE);
        write(UART_LCR, LCR_WL8 | LCR_EPS | LCR_SPB);
        EXPECT_EQ(transmit('F').parity, SERIAL_PARITY_NONE);

        write(UART_LCR, LCR_WL8);
    }

    void test_registers() {
        for (u64 addr : { UART_MCR, UART_MSR, UART_SCR }) {
            write(addr, 0xa5);
            EXPECT_EQ(read(addr), 0xa5);
            write(addr, 0);
        }

        u8 data = 0;
        EXPECT_AE(out.readw(8, data));
    }
};

TEST(serial, uart) {
    uart_test test("test");
    sc_core::sc_start();
}
