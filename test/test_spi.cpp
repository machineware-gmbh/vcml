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

#include <gtest/gtest.h>
#include "vcml.h"

class initiator: public vcml::component, public vcml::spi_bw_transport_if
{
public:
    SC_HAS_PROCESS(initiator);
    vcml::spi_initiator_socket OUT;

    void run() {
        for (vcml::u8 i = 0; i < 10; i++) {
            wait(1, sc_core::SC_SEC);
            EXPECT_EQ(2 * i, OUT->spi_transport(i));
        }

        sc_core::sc_stop();
    }

    initiator(const sc_core::sc_module_name& nm):
        vcml::component(nm), OUT("OUT") {
        OUT.bind(*this);
        SC_THREAD(run);
    }

    virtual ~initiator() {
        /* nothing to do */
    }
};

class target: public vcml::component, public vcml::spi_fw_transport_if
{
public:
    vcml::spi_target_socket IN;

    target(const sc_core::sc_module_name& nm):
        vcml::component(nm), IN("IN") {
        IN.bind(*this);
    }

    virtual ~target() {
        /* nothing to do */
    }

    virtual vcml::u8 spi_transport(vcml::u8 data) override {
        return 2 * data;
    }

};

TEST(spi, sockets) {
    initiator spi_i("SPI_I");
    target spi_t("SPI_T");
    spi_i.OUT.bind(spi_t.IN);
    sc_core::sc_start();
}
