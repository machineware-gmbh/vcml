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

class cdns_test : public test_base, public serial_host
{
public:
    serial::cdns cdns;
    tlm_initiator_socket out;
    gpio_target_socket irq;
    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    enum : u64 {
        CDNS_CR = 0x0,
        CDNS_MR = 0x4,
        CDNS_IER = 0x8,
        CDNS_IDR = 0xc,
        CDNS_IMR = 0x10,
        CDNS_ISR = 0x14,
        CDNS_BRGR = 0x18,
        CDNS_RTOR = 0x1c,
        CDNS_SR = 0x2c,
        CDNS_TXRX = 0x30,
        CDNS_BDIV = 0x34,
    };

    cdns_test(const sc_module_name& nm):
        test_base(nm),
        serial_host(),
        cdns("cdns"),
        out("out"),
        irq("irq"),
        serial_tx("serial_tx"),
        serial_rx("serial_rx") {
        out.bind(cdns.in);
        cdns.irq.bind(irq);
        cdns.serial_tx.bind(serial_rx);
        serial_tx.bind(cdns.serial_rx);
        rst.bind(cdns.rst);
        clk.bind(cdns.clk);
        EXPECT_STREQ(cdns.kind(), "vcml::serial::cdns");
    }

    MOCK_METHOD(void, serial_receive, (u8), (override));

    virtual void run_test() override {
        u32 data;

        wait(SC_ZERO_TIME);
        EXPECT_FALSE(irq) << "irq did not reset";

        // test resetting device
        ASSERT_OK(out.writew(CDNS_CR, 3));
        ASSERT_OK(out.readw(CDNS_CR, data));
        EXPECT_EQ(data, 0);

        // enable transmit
        ASSERT_OK(out.writew(CDNS_CR, bit(4)));
        ASSERT_OK(out.readw(CDNS_CR, data));
        EXPECT_EQ(data, bit(4));

        // switch to normal mode
        data = 1 | 2 << 1 | 1 << 3 | 2 << 6;
        ASSERT_OK(out.writew(CDNS_MR, data));
        EXPECT_EQ(cdns.serial_tx.data_width(), SERIAL_7_BITS);
        EXPECT_EQ(cdns.serial_tx.stop_bits(), SERIAL_STOP_2);
        EXPECT_EQ(cdns.serial_tx.parity(), SERIAL_PARITY_ODD);

        // transmit data
        EXPECT_CALL(*this, serial_receive('Y'));
        ASSERT_OK(out.writew<u32>(CDNS_TXRX, 'Y'));
        ASSERT_OK(out.readw(CDNS_ISR, data));
        EXPECT_EQ(data, bit(3) | bit(1)); // rx + tx empty

        // receive data
        ASSERT_OK(out.writew(CDNS_CR, bit(2)));
        ASSERT_OK(out.writew<u32>(CDNS_IER, bit(8)));
        ASSERT_OK(out.writew<u32>(CDNS_RTOR, 10u));
        serial_tx.send('A');
        EXPECT_TRUE(irq);

        // clear rx interrupt
        ASSERT_OK(out.writew(CDNS_ISR, bit(8)));
        EXPECT_FALSE(irq);

        // retrieve data
        ASSERT_OK(out.readw(CDNS_TXRX, data));
        EXPECT_EQ(data, 'A');
        ASSERT_OK(out.readw(CDNS_TXRX, data));
        EXPECT_EQ(data, 0);
    }
};

TEST(serial, cdns) {
    cdns_test test("test");
    sc_core::sc_start();
}
