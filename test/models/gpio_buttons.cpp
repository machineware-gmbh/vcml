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
    gpio::buttons model;
    gpio::buttons active_low;
    gpio_target_socket button0;
    gpio_target_socket button1;

    gpio_buttons_test(const sc_module_name& nm):
        test_base(nm),
        model("model"),
        active_low("active_low"),
        button0("button0"),
        button1("button1") {
        model.gpio_in[0].bind(button0);
        active_low.gpio_in[0].bind(button1);
        active_low.pressed_state = false;

        add_test("active_high", &gpio_buttons_test::test_active_high);
        add_test("active_low", &gpio_buttons_test::test_active_low);
    }

    void test_active_high() {
        stringstream ss;

        EXPECT_TRUE(model.execute("push", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pressed");
        EXPECT_TRUE(button0.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(model.execute("release", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 released");
        EXPECT_FALSE(button0.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(model.execute("pulse", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pulsed");
        EXPECT_FALSE(button0.read());

        ss.str("");
        ss.clear();
        EXPECT_FALSE(model.execute("push", { "1" }, ss));
        EXPECT_EQ(ss.str(), "button1 not connected");
    }

    void test_active_low() {
        stringstream ss;

        EXPECT_FALSE(button1.read());

        EXPECT_TRUE(active_low.execute("push", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pressed");
        EXPECT_FALSE(button1.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(active_low.execute("release", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 released");
        EXPECT_TRUE(button1.read());

        ss.str("");
        ss.clear();
        EXPECT_TRUE(active_low.execute("pulse", { "0" }, ss));
        EXPECT_EQ(ss.str(), "button0 pulsed");
        EXPECT_TRUE(button1.read());
    }
};

TEST(gpio, buttons) {
    gpio_buttons_test test("test");
    sc_core::sc_start();
}
