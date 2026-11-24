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

class mmgpio_test : public test_base
{
public:
    gpio::mmgpio mmgpio;
    tlm_initiator_socket out;
    gpio_target_socket gpio0;
    gpio_target_socket gpio5;
    gpio_target_socket gpio31;

    mmgpio_test(const sc_module_name& nm):
        test_base(nm),
        mmgpio("mmgpio"),
        out("out"),
        gpio0("gpio0"),
        gpio5("gpio5"),
        gpio31("gpio31") {
        out.bind(mmgpio.in);
        rst.bind(mmgpio.rst);
        clk.bind(mmgpio.clk);
        mmgpio.gpio_out[0].bind(gpio0);
        mmgpio.gpio_out[5].bind(gpio5);
        mmgpio.gpio_out[31].bind(gpio31);

        add_test("strings", &mmgpio_test::test_strings);
        add_test("data", &mmgpio_test::test_data);
        add_test("reset", &mmgpio_test::test_reset);
        add_test("commands", &mmgpio_test::test_commands);
    }

    u32 read() {
        u32 data = 0;
        EXPECT_OK(out.readw(0, data));
        return data;
    }

    void write(u32 data) { EXPECT_OK(out.writew(0, data)); }

    void test_strings() {
        EXPECT_STREQ(mmgpio.kind(), "vcml::gpio::mmgpio");
        EXPECT_STREQ(mmgpio.version(), VCML_VERSION_STRING);
    }

    void test_data() {
        EXPECT_EQ(read(), 0);
        EXPECT_FALSE(gpio0);
        EXPECT_FALSE(gpio5);
        EXPECT_FALSE(gpio31);

        write(bit(5));
        EXPECT_FALSE(gpio0);
        EXPECT_TRUE(gpio5);
        EXPECT_FALSE(gpio31);
        EXPECT_EQ(read(), bit(5));

        write(bit(31) | bit(0));
        EXPECT_TRUE(gpio0);
        EXPECT_FALSE(gpio5);
        EXPECT_TRUE(gpio31);
        EXPECT_EQ(read(), bit(31) | bit(0));

        write(0xffffffff);
        EXPECT_TRUE(gpio0);
        EXPECT_TRUE(gpio5);
        EXPECT_TRUE(gpio31);
        EXPECT_EQ(read(), bit(31) | bit(5) | bit(0));

        write(0);
        EXPECT_FALSE(gpio0);
        EXPECT_FALSE(gpio5);
        EXPECT_FALSE(gpio31);
        EXPECT_EQ(read(), 0);
    }

    void test_reset() {
        write(bit(31) | bit(5) | bit(0));
        EXPECT_TRUE(gpio0);
        EXPECT_TRUE(gpio5);
        EXPECT_TRUE(gpio31);

        mmgpio.reset();
        EXPECT_FALSE(gpio0);
        EXPECT_FALSE(gpio5);
        EXPECT_FALSE(gpio31);
        EXPECT_EQ(read(), 0);
    }

    void test_commands() {
        stringstream ss;
        EXPECT_TRUE(mmgpio.execute("set", { "5" }, ss));
        EXPECT_EQ(ss.str(), "setting GPIO5");
        EXPECT_TRUE(gpio5);
        EXPECT_EQ(read(), bit(5));

        ss.str("");
        EXPECT_TRUE(mmgpio.execute("set", { "0x1f" }, ss));
        EXPECT_EQ(ss.str(), "setting GPIO31");
        EXPECT_TRUE(gpio31);

        ss.str("");
        EXPECT_TRUE(mmgpio.execute("status", ss));
        EXPECT_THAT(ss.str(), HasSubstr("DATA: 0x80000020"));
        EXPECT_THAT(ss.str(), AnyOf(HasSubstr("Set: 5, 31, "),
                                    HasSubstr("Set: 31, 5, ")));
        EXPECT_THAT(ss.str(), HasSubstr("Cleared: 0, "));

        ss.str("");
        EXPECT_TRUE(mmgpio.execute("clear", { "5" }, ss));
        EXPECT_EQ(ss.str(), "clearing GPIO5");
        EXPECT_FALSE(gpio5);
        EXPECT_TRUE(mmgpio.execute("clear", { "31" }, ss));
        EXPECT_FALSE(gpio31);
        EXPECT_EQ(read(), 0);

        for (const char* cmd : { "set", "clear" }) {
            ss.str("");
            EXPECT_FALSE(mmgpio.execute(cmd, { "32" }, ss));
            EXPECT_EQ(ss.str(), "index out of bounds: 32");
            ss.str("");
            EXPECT_FALSE(mmgpio.execute(cmd, { "1" }, ss));
            EXPECT_EQ(ss.str(), "GPIO1 not connected");
            EXPECT_FALSE(mmgpio.execute(cmd, ss));
        }

        EXPECT_EQ(read(), 0);
    }
};

TEST(gpio, mmgpio) {
    mmgpio_test test("test");
    sc_core::sc_start();
}
