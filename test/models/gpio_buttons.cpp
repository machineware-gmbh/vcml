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

class gpio_buttons_test : public test_base
{
public:
    gpio::buttons button0;
    gpio::buttons button1;
    gpio_target_socket button0_target;
    gpio_target_socket button1_target;

    gpio_buttons_test(const sc_module_name& nm):
        test_base(nm),
        button0("button0"),
        button1("button1"),
        button0_target("button0_target"),
        button1_target("button1_target") {
        button0.gpio_in[0].bind(button0_target);
        button1.gpio_in[0].bind(button1_target);

        button1.pressed_state = false;

        add_test("active_high", &gpio_buttons_test::test_active_high);
        add_test("test_active_low", &gpio_buttons_test::test_active_low);
    }

    void test_active_high() {
        stringstream ss;

        EXPECT_TRUE(button0.execute("status", {}, ss));
        EXPECT_EQ(ss.str(), "button0: released\n");

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button0.execute("push", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pressed");
        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_TRUE(button0_target.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button0.execute("status", {}, ss));
        EXPECT_EQ(ss.str(), "button0: pressed\n");

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button0.execute("release", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 released");
        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_FALSE(button0_target.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button0.execute("pulse", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pulsed");
        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_FALSE(button0_target.read());

        ss.str("");
        ss.clear();
        EXPECT_FALSE(button0.execute("push", { "1" }, ss));
        EXPECT_EQ(ss.str(), "button1 not connected");
    }

    void test_active_low() {
        stringstream ss;

        EXPECT_FALSE(button1_target.read());

        EXPECT_TRUE(button1.execute("status", {}, ss));
        EXPECT_EQ(ss.str(), "button0: pressed\n");

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button1.execute("push", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pressed");
        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_FALSE(button1_target.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button1.execute("release", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 released");
        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_TRUE(button1_target.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(button1.execute("pulse", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pulsed");
        sc_core::wait(1, sc_core::SC_NS);
        EXPECT_TRUE(button1_target.read());
    }
};

TEST(gpio, buttons) {
    gpio_buttons_test test("test");
    sc_core::sc_start();
}
