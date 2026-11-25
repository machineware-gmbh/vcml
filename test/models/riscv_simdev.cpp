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

class riscv_simdev_test : public test_base
{
public:
    riscv::simdev simdev;
    tlm_initiator_socket out;

    riscv_simdev_test(const sc_module_name& nm):
        test_base(nm), simdev("simdev"), out("out") {
        out.bind(simdev.in);
        rst.bind(simdev.rst);
        clk.bind(simdev.clk);

        add_test("strings", &riscv_simdev_test::test_strings);
        add_test("illegal", &riscv_simdev_test::test_illegal);
        add_test("fail", &riscv_simdev_test::test_fail);
        add_test("pass", &riscv_simdev_test::test_pass);
    }

    u32 read() {
        u32 data = 0;
        EXPECT_OK(out.readw(0, data));
        return data;
    }

    void test_strings() {
        EXPECT_STREQ(simdev.kind(), "vcml::riscv::simdev");
        EXPECT_STREQ(simdev.version(), VCML_VERSION_STRING);
    }

    void test_illegal() {
        EXPECT_EQ(read(), 0);

        u32 data = 0x1234;
        EXPECT_OK(out.writew(0, data));
        EXPECT_EQ(read(), 0x1234);
        EXPECT_TRUE(sim_running());

        EXPECT_AE(out.readw(4, data));
    }

    void test_fail() {
#if !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
        u32 data = 7 << 16 | riscv::simdev::FINISH_FAIL;
        EXPECT_EXIT(out.writew(0, data), ExitedWithCode(7), "");
#endif
    }

    void test_pass() {
        u32 data = riscv::simdev::FINISH_PASS;
        EXPECT_OK(out.writew(0, data));
        wait(SC_ZERO_TIME);
        ADD_FAILURE() << "simulation did not stop";
    }
};

TEST(riscv, simdev) {
    riscv_simdev_test test("test");
    sc_core::sc_start();
}
