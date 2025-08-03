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
};

TEST(spi, mcp3208) {
    mcp3208test test("test");
    sc_core::sc_start();
}
