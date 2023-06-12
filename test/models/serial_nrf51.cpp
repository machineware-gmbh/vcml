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

class nrf51_test : public test_base, public serial_host
{
public:
    serial::nrf51 nrf51;
    tlm_initiator_socket out;
    gpio_target_socket irq;
    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    enum : u64 {
        NRF51_STARTRX = 0x0,
        NRF51_STOPRX = 0x4,
        NRF51_STARTTX = 0x8,
        NRF51_STOPTX = 0xc,
        NRF51_SUSPEND = 0x1c,
        NRF51_CTS = 0x100,
        NRF51_NCTS = 0x104,
        NRF51_RXDRDY = 0x108,
        NRF51_TXDRDY = 0x11c,
        NRF51_ERROR = 0x124,
        NRF51_RXTO = 0x144,
        NRF51_INTEN = 0x300,
        NRF51_INTENSET = 0x304,
        NRF51_INTENCLR = 0x308,
        NRF51_ERRSRC = 0x480,
        NRF51_ENABLE = 0x500,
        NRF51_RXD = 0x518,
        NRF51_TXD = 0x51c,
        NRF51_BAUDRATE = 0x524,
        NRF51_CONFIG = 0x56c,
    };

    nrf51_test(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
        nrf51("nrf51"),
        out("out"),
        irq("irq"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx") {
        out.bind(nrf51.in);
        nrf51.irq.bind(irq);
        nrf51.serial_tx.bind(serial_rx);
        serial_tx.bind(nrf51.serial_rx);
        rst.bind(nrf51.rst);
        clk.bind(nrf51.clk);
        EXPECT_STREQ(nrf51.kind(), "vcml::serial::nrf51");
    }

    MOCK_METHOD(void, serial_receive, (u8), (override));

    virtual void run_test() override {
        u32 data;

        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irq) << "irq did not reset";

        // test setting device up
        EXPECT_OK(out.writew<u32>(NRF51_BAUDRATE, 0x0004f000));
        EXPECT_EQ(nrf51.serial_tx.baud(), 1200) << "baud rate not set";
        EXPECT_OK(out.writew<u32>(NRF51_CONFIG, 14));
        EXPECT_EQ(nrf51.serial_tx.parity(), SERIAL_PARITY_MARK);

        // test interrupt setting
        EXPECT_OK(out.writew<u32>(NRF51_ENABLE, 4));
        EXPECT_OK(out.writew<u32>(NRF51_INTEN, bit(9)));
        EXPECT_OK(out.writew<u32>(NRF51_INTENSET, bit(7)));
        EXPECT_OK(out.readw<u32>(NRF51_INTEN, data));
        EXPECT_EQ(data, bit(9) | bit(7));
        EXPECT_OK(out.readw<u32>(NRF51_INTENCLR, data));
        EXPECT_EQ(data, bit(9) | bit(7));
        EXPECT_OK(out.writew<u32>(NRF51_INTENCLR, bit(9)));
        EXPECT_OK(out.readw<u32>(NRF51_INTENSET, data));
        EXPECT_EQ(data, bit(7));

        // test transmission
        EXPECT_OK(out.writew<u32>(NRF51_STARTTX, 1));
        EXPECT_OK(out.readw<u32>(NRF51_TXDRDY, data));
        EXPECT_EQ(data, 1);
        EXPECT_CALL(*this, serial_receive('X'));
        EXPECT_OK(out.writew<u32>(NRF51_TXD, 'X'));
        EXPECT_TRUE(irq);
        EXPECT_OK(out.readw<u32>(NRF51_TXDRDY, data));
        EXPECT_EQ(data, 1);
        EXPECT_OK(out.writew<u32>(NRF51_INTENCLR, bit(7)));
        EXPECT_FALSE(irq);

        // test reception
        EXPECT_OK(out.writew<u32>(NRF51_STARTRX, 1));
        EXPECT_OK(out.writew<u32>(NRF51_INTENSET, bit(2)));
        serial_tx.send('Y');
        EXPECT_TRUE(irq);
        EXPECT_OK(out.readw<u32>(NRF51_RXDRDY, data));
        EXPECT_EQ(data, 1);
        EXPECT_OK(out.readw<u32>(NRF51_RXD, data));
        EXPECT_EQ(data, 'Y');
        EXPECT_OK(out.readw<u32>(NRF51_RXDRDY, data));
        EXPECT_EQ(data, 0);

        // test suspend
        EXPECT_OK(out.writew<u32>(NRF51_SUSPEND, 1));
        EXPECT_OK(out.readw<u32>(NRF51_TXDRDY, data));
        EXPECT_EQ(data, 0);
        EXPECT_OK(out.readw<u32>(NRF51_RXDRDY, data));
        EXPECT_EQ(data, 0);
        EXPECT_CALL(*this, serial_receive(_)).Times(0);
        EXPECT_OK(out.writew<u32>(NRF51_TXD, 'W'));
        EXPECT_FALSE(irq);
        EXPECT_OK(out.writew<u32>(NRF51_ENABLE, 0));
    }
};

TEST(serial, nrf51) {
    nrf51_test test("test");
    sc_core::sc_start();
}
