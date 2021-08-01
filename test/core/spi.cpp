/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

class spi_harness: public test_base, public spi_host
{
public:
    spi_initiator_socket SPI_OUT;
    spi_target_socket SPI_IN;

    spi_harness(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        SPI_OUT("SPI_OUT"),
        SPI_IN("SPI_IN") {
        SPI_OUT.bind(SPI_IN);
    }

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override {
        spi.miso = 2 * spi.mosi;
    }

    virtual void run_test() override {
        for (vcml::u8 i = 0; i < 10; i++) {
            wait(1, sc_core::SC_SEC);
            spi_payload spi(i);
            SPI_OUT->spi_transport(spi);
            EXPECT_EQ(spi.miso, spi.mosi * 2);
        }
    }
};

TEST(spi, sockets) {
    spi_harness test("test");
    sc_core::sc_start();
}
