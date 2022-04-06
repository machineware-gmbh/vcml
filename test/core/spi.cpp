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

enum : address_space {
    VCML_AS_TEST = VCML_AS_DEFAULT + 1,
};

class spi_harness : public test_base, public spi_host
{
public:
    spi_initiator_socket spi_out;
    spi_target_socket spi_in;

    spi_base_initiator_socket spi_out_h;
    spi_base_target_socket spi_in_h;

    spi_initiator_socket spi_out2;
    spi_target_socket spi_in2;

    spi_harness(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        spi_out("spi_out"),
        spi_in("spi_in", VCML_AS_TEST),
        spi_out_h("spi_out_h"),
        spi_in_h("spi_in_h"),
        spi_out2("spi_out2"),
        spi_in2("spi_in2") {

        // test hierarchy binding
        spi_out.bind(spi_out_h);
        spi_in_h.bind(spi_in);
        spi_out_h.bind(spi_in_h);

        // test stubbing
        spi_out2.stub();
        spi_in2.stub();

        EXPECT_TRUE(find_object("spi.spi_out2_stub"));
        EXPECT_TRUE(find_object("spi.spi_in2_stub"));

        auto initiators = all_spi_initiator_sockets();
        auto targets    = all_spi_target_sockets();
        auto sockets    = all_spi_target_sockets(VCML_AS_TEST);

        EXPECT_EQ(initiators.size(), 2) << "spi initiators did not register";
        EXPECT_EQ(targets.size(), 2) << "spi targets did not register";
        EXPECT_EQ(sockets.size(), 1) << "spi targets in wrong address space";
    }

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override {
        EXPECT_EQ(socket.as, VCML_AS_TEST);
        spi.miso = 2 * spi.mosi;
    }

    virtual void run_test() override {
        for (vcml::u8 i = 0; i < 10; i++) {
            wait(1, sc_core::SC_SEC);
            spi_payload spi(i);
            spi_out->spi_transport(spi);
            EXPECT_EQ(spi.miso, spi.mosi * 2);
        }
    }
};

TEST(spi, sockets) {
    broker_arg broker(sc_argc(), sc_argv());
    tracer_term tracer;
    spi_harness test("spi");
    sc_core::sc_start();
}
