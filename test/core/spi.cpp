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

    spi_initiator_array spi_out_arr;
    spi_target_array spi_in_arr;

    spi_harness(const sc_module_name& nm):
        test_base(nm),
        spi_host(),
        spi_out("spi_out"),
        spi_in("spi_in", VCML_AS_TEST),
        spi_out_h("spi_out_h"),
        spi_in_h("spi_in_h"),
        spi_out_arr("spi_out_arr"),
        spi_in_arr("spi_in_arr") {
        // test hierarchy binding
        spi_bind(*this, "spi_out", *this, "spi_out_h");
        spi_bind(*this, "spi_in_h", *this, "spi_in");
        spi_bind(*this, "spi_out_h", *this, "spi_in_h");

        // test stubbing
        spi_stub(*this, "spi_out_arr", 33);
        spi_stub(*this, "spi_in_arr", 44);

        EXPECT_TRUE(find_object("spi.spi_out_arr[33]_stub"));
        EXPECT_TRUE(find_object("spi.spi_in_arr[44]_stub"));
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
    spi_harness test("spi");
    sc_core::sc_start();
}
