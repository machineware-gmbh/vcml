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

class hwrng_test : public test_base
{
public:
    generic::hwrng hwrng;
    tlm_initiator_socket out;

    hwrng_test(const sc_module_name& nm):
        test_base(nm), hwrng("hwrng"), out("out") {
        out.bind(hwrng.in);
        rst.bind(hwrng.rst);
        clk.bind(hwrng.clk);

        add_test("strings", &hwrng_test::test_strings);
        add_test("random", &hwrng_test::test_random);
        add_test("pseudo", &hwrng_test::test_pseudo);
    }

    u32 read() {
        u32 data = 0;
        EXPECT_OK(out.readw(0, data));
        return data;
    }

    void test_strings() {
        EXPECT_STREQ(hwrng.kind(), "vcml::generic::hwrng");
        EXPECT_STREQ(hwrng.version(), VCML_VERSION_STRING);
        EXPECT_FALSE(hwrng.pseudo);
        EXPECT_EQ(hwrng.seed, 0);
    }

    void test_random() {
        std::set<u32> values;
        for (size_t i = 0; i < 8; i++)
            values.insert(read());
        EXPECT_GT(values.size(), 1);

        u32 data = 0;
        EXPECT_CE(out.writew(0, data));
        EXPECT_AE(out.readw(4, data));
    }

    void test_pseudo() {
        hwrng.pseudo = true;
        hwrng.seed = 1234;

        u32 expected[4];
        srand(hwrng.seed);
        for (u32& val : expected)
            val = (u32)rand();

        for (size_t round = 0; round < 2; round++) {
            hwrng.reset();
            for (u32 val : expected)
                EXPECT_EQ(read(), val) << "round " << round;
        }

        hwrng.pseudo = false;
        hwrng.seed = 0;
    }
};

TEST(generic, hwrng) {
    hwrng_test test("test");
    sc_core::sc_start();
}
