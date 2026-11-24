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

enum : u8 {
    RELEASED = 0x80,
};

class ockbd_test : public test_base
{
public:
    opencores::ockbd ockbd;
    tlm_initiator_socket out;
    gpio_target_socket irq;

    ockbd_test(const sc_module_name& nm):
        test_base(nm), ockbd("ockbd"), out("out"), irq("irq") {
        out.bind(ockbd.in);
        rst.bind(ockbd.rst);
        clk.bind(ockbd.clk);
        ockbd.irq.bind(irq);

        add_test("strings", &ockbd_test::test_strings);
        add_test("keys", &ockbd_test::test_keys);
        add_test("shift", &ockbd_test::test_shift);
        add_test("overflow", &ockbd_test::test_overflow);
    }

    ui::keyboard& keyboard() {
        auto keyboards = ui::input::all<ui::keyboard>();
        VCML_ERROR_ON(keyboards.size() != 1, "keyboard not found");
        return *keyboards[0];
    }

    void press(u32 symbol) {
        keyboard().notify_key(symbol, true);
        keyboard().notify_key(symbol, false);
        wait(clock_cycles(2));
    }

    u8 read_khr() {
        u8 data = 0xff;
        EXPECT_OK(out.readw(0, data));
        return data;
    }

    vector<u8> read_keys() {
        vector<u8> keys;
        while (irq)
            keys.push_back(read_khr());
        return keys;
    }

    void test_strings() {
        EXPECT_STREQ(ockbd.kind(), "vcml::opencores::ockbd");
        EXPECT_STREQ(ockbd.version(), VCML_VERSION_STRING);
        EXPECT_EQ(ockbd.keymap.get(), "us");
        EXPECT_EQ(ockbd.fifosize, 16);
        EXPECT_STREQ(keyboard().layout(), "us");
    }

    void test_keys() {
        EXPECT_FALSE(irq);
        EXPECT_EQ(read_khr(), 0);

        keyboard().notify_key(ui::KEYSYM_a, true);
        wait(clock_cycles(2));
        EXPECT_TRUE(irq);
        EXPECT_EQ(read_khr(), ui::KEY_A);
        EXPECT_FALSE(irq);
        EXPECT_EQ(read_khr(), 0);

        keyboard().notify_key(ui::KEYSYM_a, false);
        wait(clock_cycles(2));
        EXPECT_TRUE(irq);
        EXPECT_EQ(read_khr(), ui::KEY_A | RELEASED);
        EXPECT_FALSE(irq);

        press(ui::KEYSYM_1);
        press(ui::KEYSYM_ENTER);
        EXPECT_TRUE(irq);
        EXPECT_EQ(read_keys(),
                  vector<u8>({ ui::KEY_1, ui::KEY_1 | RELEASED, ui::KEY_ENTER,
                               ui::KEY_ENTER | RELEASED }));
        EXPECT_FALSE(irq);

        u8 data = 0;
        EXPECT_CE(out.writew(0, data));
    }

    void test_shift() {
        press(ui::KEYSYM_A);
        EXPECT_EQ(read_keys(),
                  vector<u8>({ ui::KEY_LEFTSHIFT, ui::KEY_RIGHTSHIFT,
                               ui::KEY_A, ui::KEY_LEFTSHIFT | RELEASED,
                               ui::KEY_RIGHTSHIFT | RELEASED,
                               ui::KEY_A | RELEASED }));
        EXPECT_FALSE(irq);
    }

    void test_overflow() {
        for (size_t i = 0; i < ockbd.fifosize; i++)
            press(ui::KEYSYM_a);

        vector<u8> keys = read_keys();
        ASSERT_EQ(keys.size(), ockbd.fifosize);
        for (size_t i = 0; i < keys.size(); i++)
            EXPECT_EQ(keys[i], i % 2 ? ui::KEY_A | RELEASED : ui::KEY_A);

        EXPECT_FALSE(irq);
        EXPECT_EQ(read_khr(), 0);

        press(ui::KEYSYM_1);
        EXPECT_EQ(read_keys(),
                  vector<u8>({ ui::KEY_1, ui::KEY_1 | RELEASED }));
    }
};

TEST(opencores, ockbd) {
    ockbd_test test("test");
    sc_core::sc_start();
}
