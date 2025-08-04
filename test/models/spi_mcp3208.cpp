/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class mcp3208test : public test_base
{
public:
    spi::mcp3208 mcp3208;
    spi_initiator_socket spi;
    gpio_initiator_socket cs;

    mcp3208test(const sc_module_name& nm):
        test_base(nm), mcp3208("mcp3208"), spi("spi"), cs("cs") {
        spi.bind(mcp3208.spi_in);
        cs.bind(mcp3208.spi_cs);

        add_test("strings", &mcp3208test::test_strings);
        add_test("transfer_no_cs", &mcp3208test::test_transfer_no_cs);
        add_test("transfer_1", &mcp3208test::test_transfer_1);
        add_test("transfer_2", &mcp3208test::test_transfer_2);
        add_test("commands_get", &mcp3208test::test_commands_get);
        add_test("commands_set", &mcp3208test::test_commands_set);
    }

    void test_strings() {
        EXPECT_STREQ(mcp3208.kind(), "vcml::spi::mcp3208");
        EXPECT_STREQ(mcp3208.version(), VCML_VERSION_STRING);
    }

    void test_transfer_no_cs() {
        cs = !mcp3208.csmode;

        const u8 tx[3] = { 0x70, 0x00, 0x00 };
        const u8 rx[3] = { 0x00, 0x00, 0x00 };

        for (int i = 0; i < 3; i++) {
            spi_payload payload{};
            payload.mosi = tx[i];
            spi.transport(payload);
            EXPECT_EQ(payload.miso, rx[i]);
        }
    }

    void test_transfer_1() {
        cs = mcp3208.csmode;

        mcp3208.vref = 5.0;
        mcp3208.v4 = 3.3;

        spi_payload payload{};

        // linux uses one leading and 4 trailing zeroes
        const u8 tx[3] = { 0x70, 0x00, 0x00 };
        const u8 rx[3] = { 0x00, 0xa8, 0xe0 };

        for (int i = 0; i < 3; i++) {
            payload.mosi = tx[i];
            spi.transport(payload);
            EXPECT_EQ(payload.miso, rx[i]) << "position " << i;
        }

        cs = !mcp3208.csmode;
    }

    void test_transfer_2() {
        cs = mcp3208.csmode;

        mcp3208.vref = 5.0;
        mcp3208.v4 = 3.3;

        // zephyr uses 5 leading an no trailing zeroes
        const u8 tx[3] = { 0x07, 0x00, 0x00 };
        const u8 rx[3] = { 0x00, 0x0a, 0x8e };

        for (int i = 0; i < 3; i++) {
            spi_payload payload{};
            payload.mosi = tx[i];
            spi.transport(payload);
            EXPECT_EQ(payload.miso, rx[i]);
        }

        cs = !mcp3208.csmode;
    }

    void test_commands_get() {
        stringstream ss;
        EXPECT_TRUE(mcp3208.execute("get_voltage", ss));
        EXPECT_EQ(ss.str(), mkstr("vref: %.3f\n", mcp3208.vref.get()) +
                                mkstr("v0: %.3f\n", mcp3208.v0.get()) +
                                mkstr("v1: %.3f\n", mcp3208.v1.get()) +
                                mkstr("v2: %.3f\n", mcp3208.v2.get()) +
                                mkstr("v3: %.3f\n", mcp3208.v3.get()) +
                                mkstr("v4: %.3f\n", mcp3208.v4.get()) +
                                mkstr("v5: %.3f\n", mcp3208.v5.get()) +
                                mkstr("v6: %.3f\n", mcp3208.v6.get()) +
                                mkstr("v7: %.3f", mcp3208.v7.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "vref" }, ss));
        EXPECT_EQ(ss.str(), mkstr("vref: %.3f", mcp3208.vref.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v0" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v0: %.3f", mcp3208.v0.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v1" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v1: %.3f", mcp3208.v1.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v2" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v2: %.3f", mcp3208.v2.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v3" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v3: %.3f", mcp3208.v3.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v4" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v4: %.3f", mcp3208.v4.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v5" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v5: %.3f", mcp3208.v5.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v6" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v6: %.3f", mcp3208.v6.get()));

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", { "v7" }, ss));
        EXPECT_EQ(ss.str(), mkstr("v7: %.3f", mcp3208.vref.get()));

        ss.str("");
        EXPECT_FALSE(mcp3208.execute("get_voltage", { "xxx" }, ss));
        EXPECT_EQ(ss.str(),
                  "unknown channel: xxx\n"
                  "use: vref, v0, v1, v2, v3, v4, v5, v6, v7");
    }

    void test_commands_set() {
        stringstream ss;
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v0", "0.0" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v1", "0.1" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v2", "0.2" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v3", "0.3" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v4", "0.4" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v5", "0.5" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v6", "0.6" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "v7", "0.7" }, ss));
        EXPECT_TRUE(mcp3208.execute("set_voltage", { "vref", "1.0" }, ss));

        EXPECT_EQ(mcp3208.vref, 1.0);
        EXPECT_EQ(mcp3208.v0, 0.0);
        EXPECT_EQ(mcp3208.v1, 0.1);
        EXPECT_EQ(mcp3208.v2, 0.2);
        EXPECT_EQ(mcp3208.v3, 0.3);
        EXPECT_EQ(mcp3208.v4, 0.4);
        EXPECT_EQ(mcp3208.v5, 0.5);
        EXPECT_EQ(mcp3208.v6, 0.6);
        EXPECT_EQ(mcp3208.v7, 0.7);

        ss.str("");
        EXPECT_FALSE(mcp3208.execute("set_voltage", { "xxx", "0.0" }, ss));
        EXPECT_EQ(ss.str(),
                  "unknown channel: xxx\n"
                  "use: vref, v0, v1, v2, v3, v4, v5, v6, v7");

        ss.str("");
        EXPECT_TRUE(mcp3208.execute("get_voltage", ss));
        EXPECT_EQ(ss.str(),
                  "vref: 1.000\n"
                  "v0: 0.000\n"
                  "v1: 0.100\n"
                  "v2: 0.200\n"
                  "v3: 0.300\n"
                  "v4: 0.400\n"
                  "v5: 0.500\n"
                  "v6: 0.600\n"
                  "v7: 0.700");
    }
};

TEST(spi, mcp3208) {
    mcp3208test test("test");
    sc_core::sc_start();
}
