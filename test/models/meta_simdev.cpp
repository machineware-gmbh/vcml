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

enum : u64 {
    SIMDEV_STOP = 0x00,
    SIMDEV_EXIT = 0x08,
    SIMDEV_ABRT = 0x10,
    SIMDEV_SCLK = 0x18,
    SIMDEV_HCLK = 0x20,
    SIMDEV_SOUT = 0x28,
    SIMDEV_SERR = 0x30,
    SIMDEV_PRNG = 0x38,
};

class simdev_test : public test_base
{
public:
    meta::simdev simdev;
    tlm_initiator_socket out;

    simdev_test(const sc_module_name& nm):
        test_base(nm), simdev("simdev"), out("out") {
        out.bind(simdev.in);
        rst.bind(simdev.rst);
        clk.bind(simdev.clk);

        add_test("strings", &simdev_test::test_strings);
        add_test("clocks", &simdev_test::test_clocks);
        add_test("output", &simdev_test::test_output);
        add_test("random", &simdev_test::test_random);
        add_test("exit", &simdev_test::test_exit);
        add_test("stop", &simdev_test::test_stop);
    }

    void write(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void test_strings() {
        EXPECT_STREQ(simdev.kind(), "vcml::meta::simdev");
        EXPECT_STREQ(simdev.version(), VCML_VERSION_STRING);
    }

    void test_clocks() {
        wait(sc_time(1234, SC_NS));

        u64 sclk = 0;
        EXPECT_OK(out.readw(SIMDEV_SCLK, sclk));
        EXPECT_EQ(sclk, sc_time_stamp().value());

        wait(sc_time(1, SC_US));
        u64 later = 0;
        EXPECT_OK(out.readw(SIMDEV_SCLK, later));
        EXPECT_EQ(later - sclk, sc_time(1, SC_US).value());

        u64 before = mwr::timestamp_us();
        u64 hclk = 0;
        EXPECT_OK(out.readw(SIMDEV_HCLK, hclk));
        u64 after = mwr::timestamp_us();
        EXPECT_GE(hclk, before);
        EXPECT_LE(hclk, after);
    }

    void test_output() {
        testing::internal::CaptureStdout();
        for (char c : string("hello"))
            write(SIMDEV_SOUT, c);
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "hello");

        testing::internal::CaptureStderr();
        for (char c : string("world"))
            write(SIMDEV_SERR, c);
        EXPECT_EQ(testing::internal::GetCapturedStderr(), "world");
    }

    void test_random() {
        srand(1234);
        u32 expected[4];
        for (u32& val : expected)
            val = rand();

        srand(1234);
        for (u32 val : expected) {
            u32 prng = 0;
            EXPECT_OK(out.readw(SIMDEV_PRNG, prng));
            EXPECT_EQ(prng, val);
        }
    }

    void test_exit() {
#if !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
        u32 code = 3;
        EXPECT_EXIT(out.writew(SIMDEV_EXIT, code), ExitedWithCode(3), "");
        EXPECT_DEATH(out.writew(SIMDEV_ABRT, code), "");
#endif
    }

    void test_stop() {
        EXPECT_TRUE(sim_running());
        write(SIMDEV_STOP, 0);
        wait(SC_ZERO_TIME);
        ADD_FAILURE() << "simulation did not stop";
    }
};

TEST(meta, simdev) {
    simdev_test test("test");
    sc_core::sc_start();
}
