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

class pl022test : public test_base, public spi_host
{
public:
    spi::pl022 pl022;

    fifo<u16> mosi;
    fifo<u16> miso;

    tlm_initiator_socket out;
    spi_target_socket spi;
    gpio_target_socket cs;
    clk_target_socket sclk;

    gpio_target_socket intr;
    gpio_target_socket txintr;
    gpio_target_socket rxintr;
    gpio_target_socket rorintr;
    gpio_target_socket rtintr;

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& tx) override {
        mosi.push(tx.mosi);
        if (!miso.empty())
            tx.miso = miso.pop();

        EXPECT_FALSE(tx.mosi & ~tx.mask);
        EXPECT_FALSE(tx.miso & ~tx.mask);
    }

    pl022test(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        pl022("pl022"),
        mosi(8),
        miso(8),
        out("out"),
        spi("spi"),
        cs("cs"),
        sclk("sclk"),
        intr("intr"),
        txintr("txintr"),
        rxintr("rxintr"),
        rorintr("rorintr"),
        rtintr("rtintr") {
        rst.bind(pl022.rst);
        clk.bind(pl022.clk);
        out.bind(pl022.in);

        pl022.spi_out.bind(spi);
        pl022.spi_cs.bind(cs);
        pl022.sclk.bind(sclk);
        pl022.intr.bind(intr);
        pl022.txintr.bind(txintr);
        pl022.rxintr.bind(rxintr);
        pl022.rorintr.bind(rorintr);
        pl022.rtintr.bind(rtintr);

        add_test("strings", &pl022test::test_strings);
        add_test("txrx", &pl022test::test_txrx);
    }

    void test_strings() {
        EXPECT_STREQ(pl022.kind(), "vcml::spi::pl022");
        EXPECT_STREQ(pl022.version(), VCML_VERSION_STRING);
    }

    enum pl022_addresses : u64 {
        ADDR_CR0 = 0x00,
        ADDR_CR1 = 0x04,
        ADDR_DR = 0x08,
        ADDR_SR = 0x0c,
        ADDR_CPSR = 0x10,
    };

    void test_txrx() {
        // init
        ASSERT_OK(out.writew<u16>(ADDR_CR1, 0u)); // disable SPI
        ASSERT_OK(out.writew<u16>(ADDR_CR0, 9u)); // 10bits, spi frame format
        ASSERT_OK(out.writew<u16>(ADDR_CPSR, 2)); // clock prescale
        ASSERT_OK(out.writew<u16>(ADDR_CR1, 2u)); // enable SPI

        // check serial clock
        EXPECT_EQ(sclk, clk / 2);

        // wait until ready
        u16 sr = 0;
        ASSERT_OK(out.readw<u16>(ADDR_SR, sr));
        ASSERT_TRUE(sr & bit(1)); // txff not full

        // send data
        u16 txdata = 0x345;
        u16 rxdata = 0x321;
        ASSERT_EQ(fls(txdata), 9);
        ASSERT_EQ(fls(rxdata), 9);
        miso.push(rxdata);
        ASSERT_OK(out.writew<u16>(ADDR_DR, txdata));
        wait(1, SC_MS);
        ASSERT_FALSE(mosi.empty());
        ASSERT_EQ(mosi.top(), txdata);

        // receive data
        u16 data = 0;
        ASSERT_OK(out.readw<u16>(ADDR_DR, data));
        EXPECT_EQ(data, rxdata);
        EXPECT_TRUE(miso.empty());
    }
};

TEST(spi, pl022) {
    pl022test test("test");
    sc_core::sc_start();
}
