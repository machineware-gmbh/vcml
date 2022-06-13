/******************************************************************************
 *                                                                            *
 * Copyright 2022 Simon Winther, Jan Henrik Weinstock                         *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class max31855_bench : public test_base
{
public:
    generic::max31855 max31855;
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
        wait(SC_ZERO_TIME);
        for (int i = 0; i < 4; i++) {
            payload.mosi = 0;
            spi.transport(payload);
            EXPECT_EQ(payload.miso, byte[i]);
        }
        cs = false;
        wait(SC_ZERO_TIME);
    }

    void test_cs() {
        spi_payload payload(0);
        max31855.temp_thermalcouple = 25.0;
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
        max31855.temp_thermalcouple = 25.0;
        max31855.temp_internal = 25.0;
        max31855.fault = false;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;
        const u8 data1[4] = { 0b00000001, 0b10010000, 0b00011001, 0b00000000 };
        test_read(data1);

        max31855.temp_thermalcouple = -0.25;
        max31855.temp_internal = -0.0625;
        max31855.fault = true;
        max31855.scv = true;
        max31855.scg = true;
        max31855.oc = true;
        const u8 data2[4] = { 0b11111111, 0b11111101, 0b11111111, 0b11110111 };
        test_read(data2);

        max31855.temp_thermalcouple = 4096.0;
        max31855.temp_internal = 256.0;
        max31855.fault = false;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;
        const u8 data3[4] = { 0b01111111, 0b11111100, 0b01111111, 0b11110000 };
        test_read(data3);

        max31855.temp_thermalcouple = -4096.0;
        max31855.temp_internal = -256.0;
        max31855.fault = false;
        max31855.scv = false;
        max31855.scg = false;
        max31855.oc = false;
        const u8 data4[4] = { 0b10000000, 0b00000000, 0b10000000, 0b00000000 };
        test_read(data4);

        test_cs();
    }
};

TEST(max31855, simulate) {
    max31855_bench test("bench");
    sc_core::sc_start();
}
