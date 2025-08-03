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

class ads1015test : public test_base
{
public:
    i2c::ads1015 ads1015;

    i2c_initiator_socket i2c_out;
    gpio_target_socket alert_in;

    ads1015test(const sc_module_name& nm):
        test_base(nm),
        ads1015("ads1015", 0x55),
        i2c_out("i2c_out"),
        alert_in("alert_in") {
        i2c_out.bind(ads1015.i2c_in);
        ads1015.alert.bind(alert_in);
        add_test("strings", &ads1015test::test_strings);
    }

    void test_strings() {
        EXPECT_STREQ(ads1015.kind(), "vcml::i2c::ads1015");
        EXPECT_STREQ(ads1015.version(), VCML_VERSION_STRING);
    }
};

TEST(i2c, ads1015) {
    ads1015test test("test");
    sc_core::sc_start();
}
