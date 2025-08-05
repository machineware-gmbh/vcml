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

    fifo<u8> mosi;
    fifo<u8> miso;

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
    }

    void test_strings() {
        EXPECT_STREQ(pl022.kind(), "vcml::spi::pl022");
        EXPECT_STREQ(pl022.version(), VCML_VERSION_STRING);
    }
};

TEST(spi, pl022) {
    pl022test test("test");
    sc_core::sc_start();
}
