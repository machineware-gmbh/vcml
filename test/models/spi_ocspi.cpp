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
    OCSPI_RXDATA = 0x00,
    OCSPI_TXDATA = 0x04,
    OCSPI_STATUS = 0x08,
    OCSPI_CONTROL = 0x0c,
    OCSPI_BAUDDIV = 0x10,
};

enum : u8 {
    STATUS_TXE = spi::ocspi::STATUS_TXE,
    STATUS_TXR = spi::ocspi::STATUS_TXR,
};

class ocspi_test : public test_base, public spi_host
{
public:
    spi::ocspi ocspi;
    tlm_initiator_socket out;
    spi_target_socket spi_in;
    gpio_target_socket irq;

    vector<u8> mosi;
    u8 miso;

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& tx) override {
        EXPECT_EQ(tx.mask, 0xff);
        mosi.push_back(tx.mosi);
        tx.miso = miso;
    }

    ocspi_test(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        ocspi("ocspi"),
        out("out"),
        spi_in("spi_in"),
        irq("irq"),
        mosi(),
        miso() {
        out.bind(ocspi.in);
        rst.bind(ocspi.rst);
        clk.bind(ocspi.clk);
        ocspi.spi_out.bind(spi_in);
        ocspi.irq.bind(irq);

        add_test("strings", &ocspi_test::test_strings);
        add_test("defaults", &ocspi_test::test_defaults);
        add_test("transfer", &ocspi_test::test_transfer);
        add_test("interrupts", &ocspi_test::test_interrupts);
        add_test("config", &ocspi_test::test_config);
    }

    u8 read8(u64 addr) {
        u8 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write8(u64 addr, u8 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    u32 read32(u64 addr) {
        u32 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write32(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void test_strings() {
        EXPECT_STREQ(ocspi.kind(), "vcml::spi::ocspi");
        EXPECT_STREQ(ocspi.version(), VCML_VERSION_STRING);
        EXPECT_EQ(ocspi.clock, 50 * MHz);
    }

    void test_defaults() {
        EXPECT_EQ(read8(OCSPI_RXDATA), 0);
        EXPECT_EQ(read8(OCSPI_STATUS), STATUS_TXE | STATUS_TXR);
        EXPECT_EQ(read32(OCSPI_CONTROL), 0);
        EXPECT_EQ(read32(OCSPI_BAUDDIV), 0);
        EXPECT_FALSE(irq);

        u8 data = 0;
        EXPECT_CE(out.writew(OCSPI_RXDATA, data));
    }

    void test_transfer() {
        mosi.clear();
        miso = 0x5a;
        write8(OCSPI_TXDATA, 0xa5);
        EXPECT_EQ(mosi, vector<u8>({ 0xa5 }));
        EXPECT_EQ(read8(OCSPI_RXDATA), 0x5a);
        EXPECT_EQ(read8(OCSPI_RXDATA), 0x5a);
        EXPECT_EQ(read8(OCSPI_STATUS), STATUS_TXE | STATUS_TXR);
        EXPECT_FALSE(irq);

        miso = 0x11;
        write8(OCSPI_TXDATA, 0x01);
        miso = 0x22;
        write8(OCSPI_TXDATA, 0x02);
        EXPECT_EQ(mosi, vector<u8>({ 0xa5, 0x01, 0x02 }));
        EXPECT_EQ(read8(OCSPI_RXDATA), 0x22);
        EXPECT_FALSE(irq);
    }

    void test_interrupts() {
        for (u8 mask : { STATUS_TXE, STATUS_TXR }) {
            write8(OCSPI_STATUS, mask);
            EXPECT_TRUE(irq);
            write8(OCSPI_STATUS, 0);
            EXPECT_FALSE(irq);

            write8(OCSPI_TXDATA, 0x33);
            EXPECT_FALSE(irq);

            write8(OCSPI_STATUS, mask);
            write8(OCSPI_TXDATA, 0x44);
            EXPECT_TRUE(irq);
            write8(OCSPI_STATUS, 0);
            EXPECT_FALSE(irq);
        }

        write8(OCSPI_STATUS, STATUS_TXE | STATUS_TXR);
        EXPECT_TRUE(irq);
        write8(OCSPI_STATUS, STATUS_TXR);
        EXPECT_TRUE(irq);
        write8(OCSPI_STATUS, 0);
        EXPECT_FALSE(irq);
    }

    void test_config() {
        for (u32 mode = 0; mode < 4; mode++) {
            write32(OCSPI_CONTROL, mode);
            EXPECT_EQ(read32(OCSPI_CONTROL), mode);
        }

        write32(OCSPI_CONTROL, 4);
        EXPECT_EQ(read32(OCSPI_CONTROL), 0);

        write32(OCSPI_BAUDDIV, 9);
        EXPECT_EQ(read32(OCSPI_BAUDDIV), 9);
        write32(OCSPI_BAUDDIV, 9);
        EXPECT_EQ(read32(OCSPI_BAUDDIV), 9);
        write32(OCSPI_BAUDDIV, 0xffffffff);
        EXPECT_EQ(read32(OCSPI_BAUDDIV), 0xffffffff);
    }
};

TEST(spi, ocspi) {
    ocspi_test test("test");
    sc_core::sc_start();
}
