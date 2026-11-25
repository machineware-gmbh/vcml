/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class test_harness : public test_base
{
public:
    spi::flash flash;
    spi_initiator_socket spi_out;
    gpio_initiator_socket cs_out;

    test_harness(const sc_module_name& nm):
        test_base(nm), flash("flash"), spi_out("spi_out"), cs_out("cs_out") {
        spi_out.bind(flash.spi_in);
        cs_out.bind(flash.cs_in);
        rst.bind(flash.rst);
        clk.bind(flash.clk);

        add_test("strings", &test_harness::test_strings);
        add_test("ident", &test_harness::test_ident);
        add_test("write_disable", &test_harness::test_write_disable);
        add_test("write_enable", &test_harness::test_write_enable);
    }

    void spi_send(u8 data) {
        spi_payload tx(data);
        spi_out.transport(tx);
        EXPECT_EQ(tx.mosi, data);
    }

    u8 spi_recv() {
        spi_payload tx(0xff);
        spi_out.transport(tx);
        return tx.miso;
    }

    void test_strings() {
        EXPECT_STREQ(flash.kind(), "vcml::spi::flash");
        EXPECT_STREQ(flash.version(), VCML_VERSION_STRING);
    }

    void test_ident() {
        flash.reset();
        cs_out.raise();

        spi_send(0x9f); // READ_IDENT
        u32 jedec_id = spi_recv() << 16 | spi_recv() << 8 | spi_recv();
        u32 jedec_ex = spi_recv() << 8 | spi_recv();
        EXPECT_EQ(jedec_id, 0x202014);
        EXPECT_EQ(jedec_ex, 0);
    }

    void test_write_disable() {
        flash.reset();
        cs_out.raise();

        spi_send(0x04); // WRITE_DISABLE
        spi_send(0x05); // READ_STATUS
        u8 status = spi_recv();
        EXPECT_EQ(status, bit(7));
    }

    void test_write_enable() {
        flash.reset();
        cs_out.raise();

        spi_send(0x06); // WRITE_ENABLE
        spi_send(0x05); // READ_STATUS
        u8 status = spi_recv();
        EXPECT_EQ(status, 0);
    }
};

TEST(generic_memory, access) {
    test_harness test("spi");
    sc_core::sc_start();
}
