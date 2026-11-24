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

class spi_bus_test : public test_base, public spi_host
{
public:
    spi::bus bus;
    spi_initiator_socket spi_out;
    spi_target_socket spi_in0;
    spi_target_socket spi_in1;
    gpio_initiator_socket cs0;
    gpio_initiator_socket cs1;

    unsigned int port0;
    unsigned int port1;
    vector<u8> mosi0;
    vector<u8> mosi1;

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& tx) override {
        if (&socket == &spi_in0) {
            mosi0.push_back(tx.mosi);
            tx.miso = 0xa0;
        } else {
            mosi1.push_back(tx.mosi);
            tx.miso = 0xb1;
        }
    }

    spi_bus_test(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        bus("bus"),
        spi_out("spi_out"),
        spi_in0("spi_in0"),
        spi_in1("spi_in1"),
        cs0("cs0"),
        cs1("cs1"),
        port0(),
        port1(),
        mosi0(),
        mosi1() {
        rst.bind(bus.rst);
        clk.bind(bus.clk);
        bus.bind(spi_out);
        EXPECT_EQ(bus.next_free(), 0);
        port0 = bus.bind(spi_in0, cs0, true);
        EXPECT_EQ(bus.next_free(), 1);
        port1 = bus.bind(spi_in1, cs1, false);
        EXPECT_EQ(bus.next_free(), 2);

        add_test("strings", &spi_bus_test::test_strings);
        add_test("ports", &spi_bus_test::test_ports);
        add_test("transport", &spi_bus_test::test_transport);
        add_test("polarity", &spi_bus_test::test_polarity);
    }

    u8 transfer(u8 mosi) {
        spi_payload tx(mosi, 0xee);
        spi_out.transport(tx);
        return tx.miso;
    }

    void deselect() {
        cs0 = !bus.is_active_high(port0);
        cs1 = !bus.is_active_high(port1);
        mosi0.clear();
        mosi1.clear();
    }

    void test_strings() {
        EXPECT_STREQ(bus.kind(), "vcml::spi::bus");
        EXPECT_STREQ(bus.version(), VCML_VERSION_STRING);
    }

    void test_ports() {
        EXPECT_EQ(port0, 0);
        EXPECT_EQ(port1, 1);
        EXPECT_TRUE(bus.is_valid(port0));
        EXPECT_TRUE(bus.is_valid(port1));
        EXPECT_FALSE(bus.is_valid(2));

        EXPECT_TRUE(bus.is_active_high(port0));
        EXPECT_FALSE(bus.is_active_low(port0));
        EXPECT_FALSE(bus.is_active_high(port1));
        EXPECT_TRUE(bus.is_active_low(port1));

        EXPECT_FALSE(bus.is_active(2));
        EXPECT_FALSE(bus.is_active_high(2));
        EXPECT_FALSE(bus.is_active_low(2));
    }

    void test_transport() {
        deselect();
        EXPECT_FALSE(bus.is_active(port0));
        EXPECT_FALSE(bus.is_active(port1));
        EXPECT_EQ(transfer(0x01), 0xee);
        EXPECT_TRUE(mosi0.empty());
        EXPECT_TRUE(mosi1.empty());

        cs0 = true;
        EXPECT_TRUE(bus.is_active(port0));
        EXPECT_EQ(transfer(0x02), 0xa0);
        EXPECT_EQ(transfer(0x03), 0xa0);
        EXPECT_EQ(mosi0, vector<u8>({ 0x02, 0x03 }));
        EXPECT_TRUE(mosi1.empty());

        cs0 = false;
        cs1 = false;
        EXPECT_FALSE(bus.is_active(port0));
        EXPECT_TRUE(bus.is_active(port1));
        EXPECT_EQ(transfer(0x04), 0xb1);
        EXPECT_EQ(mosi0, vector<u8>({ 0x02, 0x03 }));
        EXPECT_EQ(mosi1, vector<u8>({ 0x04 }));

        cs0 = true;
        transfer(0x05);
        EXPECT_EQ(mosi0, vector<u8>({ 0x02, 0x03, 0x05 }));
        EXPECT_EQ(mosi1, vector<u8>({ 0x04, 0x05 }));

        deselect();
    }

    void test_polarity() {
        deselect();
        bus.set_active_low(port0);
        bus.set_active_high(port1);
        EXPECT_TRUE(bus.is_active_low(port0));
        EXPECT_TRUE(bus.is_active_high(port1));
        EXPECT_TRUE(bus.is_active(port0));
        EXPECT_TRUE(bus.is_active(port1));

        deselect();
        EXPECT_EQ(transfer(0x06), 0xee);
        cs1 = true;
        EXPECT_EQ(transfer(0x07), 0xb1);
        EXPECT_TRUE(mosi0.empty());
        EXPECT_EQ(mosi1, vector<u8>({ 0x07 }));

        bus.set_active_high(port0);
        bus.set_active_low(port1);
        EXPECT_TRUE(bus.is_active_high(port0));
        EXPECT_TRUE(bus.is_active_low(port1));
        deselect();
    }
};

TEST(spi, bus) {
    spi_bus_test test("test");
    sc_core::sc_start();
}
