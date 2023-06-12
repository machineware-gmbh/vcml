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

class max31855_bench : public test_base
{
public:
    spi::max31855 max31855;
    spi_initiator_socket spi;
    gpio_initiator_socket cs;

    max31855_bench(const sc_module_name& nm):
        test_base(nm), max31855("max31855"), spi("spi"), cs("cs") {
        spi.bind(max31855.spi_in);
        max31855.bind(cs, true);
    }

    void test_read(const u8 byte[4]) {
        spi_payload payload(0);
        cs = true;
        for (int i = 0; i < 4; i++) {
            payload.mosi = 0;
            spi.transport(payload);
            EXPECT_EQ(payload.miso, byte[i]);
        }
        cs = false;
    }

    void test_cs() {
        spi_payload payload(0);
        max31855.temp_thermocouple = 25.0;
        max31855.temp_internal = 0.0;
        max31855.fault = true;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;

        cs = true;
        wait(SC_ZERO_TIME);

        payload.mosi = 0;
        spi.transport(payload);
        EXPECT_EQ(payload.miso, 0b00000001);
        payload.mosi = 0;
        spi.transport(payload);
        EXPECT_EQ(payload.miso, 0b10010001);

        cs = false;
        wait(SC_ZERO_TIME);
        cs = true;
        wait(SC_ZERO_TIME);

        payload.mosi = 0;
        spi.transport(payload);
        EXPECT_EQ(payload.miso, 0b00000001);
        payload.mosi = 0;
        spi.transport(payload);
        EXPECT_EQ(payload.miso, 0b10010001);
        cs = false;
        wait(SC_ZERO_TIME);
    }

    virtual void run_test() override {
        // Test temperature reading
        max31855.temp_thermocouple = 25.0;
        max31855.temp_internal = 25.0;
        max31855.fault = false;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;
        const u8 data1[4] = { 0b00000001, 0b10010000, 0b00011001, 0b00000000 };
        test_read(data1);

        // Test negative temperature reading
        max31855.temp_thermocouple = -0.25;
        max31855.temp_internal = -0.0625;
        max31855.fault = true;
        max31855.scv = true;
        max31855.scg = true;
        max31855.oc = true;
        const u8 data2[4] = { 0b11111111, 0b11111101, 0b11111111, 0b11110111 };
        test_read(data2);

        // Test restricting to max temperature
        max31855.temp_thermocouple = 4096.0;
        max31855.temp_internal = 256.0;
        max31855.fault = false;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;
        const u8 data3[4] = { 0b01111111, 0b11111100, 0b01111111, 0b11110000 };
        test_read(data3);

        // Test restricting to min temperature
        max31855.temp_thermocouple = -4096.0;
        max31855.temp_internal = -256.0;
        max31855.fault = false;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;
        const u8 data4[4] = { 0b10000000, 0b00000000, 0b10000000, 0b00000000 };
        test_read(data4);

        // Test Chip Select
        test_cs();
    }
};

TEST(max31855, simulate) {
    max31855_bench test("bench");
    sc_core::sc_start();
}
