/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class gpio_sifive_test : public test_base
{
public:
    gpio::sifive model;

    tlm_initiator_socket out;
    gpio_initiator_socket gpio1;
    gpio_target_socket gpio3;
    gpio_target_socket irq;

    gpio_sifive_test(const sc_module_name& nm):
        test_base(nm),
        model("model"),
        out("out"),
        gpio1("gpio1"),
        gpio3("gpio3"),
        irq("irq") {
        out.bind(model.in);
        clk.bind(model.clk);
        rst.bind(model.rst);

        model.gpio_out[3].bind(gpio3);
        gpio1.bind(model.gpio_in[1]);
        model.irq[1].bind(irq);

        EXPECT_STREQ(model.kind(), "vcml::gpio::sifive");
    }

    enum : u64 {
        ADDR_INPUT_VAL = 0x00,
        ADDR_INPUT_EN = 0x04,
        ADDR_OUTPUT_EN = 0x8,
        ADDR_OUTPUT_VAL = 0x0c,
        ADDR_RISE_IE = 0x18,
        ADDR_RISE_IP = 0x1c,
        ADDR_FALL_IE = 0x20,
        ADDR_FALL_IP = 0x24,
        ADDR_HIGH_IE = 0x28,
        ADDR_HIGH_IP = 0x2c,
        ADDR_LOW_IE = 0x30,
        ADDR_LOW_IP = 0x34,
        ADDR_OUT_XOR = 0x40,
    };

    void test_output() {
        GTEST_LOG_(INFO) << "testing output GPIOs";

        u32 data;
        ASSERT_FALSE(gpio3);
        ASSERT_OK(out.writew<u32>(ADDR_OUTPUT_VAL, bit(3)));
        ASSERT_FALSE(gpio3);
        ASSERT_OK(out.writew<u32>(ADDR_OUTPUT_EN, bit(3)));
        ASSERT_TRUE(gpio3);
        ASSERT_OK(out.readw<u32>(ADDR_OUTPUT_VAL, data));
        ASSERT_EQ(data, bit(3));
        ASSERT_OK(out.writew<u32>(ADDR_OUT_XOR, bit(3)));
        ASSERT_FALSE(gpio3);
        ASSERT_OK(out.writew<u32>(ADDR_OUT_XOR, 0));
        ASSERT_TRUE(gpio3);
        ASSERT_OK(out.writew<u32>(ADDR_OUTPUT_VAL, 0));
        ASSERT_OK(out.writew<u32>(ADDR_OUTPUT_EN, 0));
        ASSERT_FALSE(gpio3);

        GTEST_LOG_(INFO) << "test complete";
    }

    void test_input() {
        GTEST_LOG_(INFO) << "testing input GPIOs";

        u32 data;
        ASSERT_OK(out.readw<u32>(ADDR_INPUT_VAL, data));
        ASSERT_EQ(data, 0u);

        gpio1 = true;
        ASSERT_OK(out.readw<u32>(ADDR_INPUT_VAL, data));
        ASSERT_EQ(data, 0u);
        ASSERT_OK(out.writew<u32>(ADDR_INPUT_EN, bit(1)));
        ASSERT_OK(out.readw<u32>(ADDR_INPUT_VAL, data));
        ASSERT_EQ(data, bit(1));
        ASSERT_OK(out.readw<u32>(ADDR_RISE_IP, data));
        ASSERT_EQ(data, bit(1));
        ASSERT_OK(out.readw<u32>(ADDR_FALL_IP, data));
        ASSERT_EQ(data, 0);
        ASSERT_OK(out.readw<u32>(ADDR_HIGH_IP, data));
        ASSERT_EQ(data, bit(1));
        ASSERT_OK(out.readw<u32>(ADDR_LOW_IP, data));
        ASSERT_EQ(data, bit(1));

        gpio1 = false;
        ASSERT_OK(out.readw<u32>(ADDR_INPUT_VAL, data));
        ASSERT_EQ(data, 0u);
        ASSERT_OK(out.readw<u32>(ADDR_RISE_IP, data));
        ASSERT_EQ(data, bit(1));
        ASSERT_OK(out.readw<u32>(ADDR_FALL_IP, data));
        ASSERT_EQ(data, bit(1));
        ASSERT_OK(out.readw<u32>(ADDR_HIGH_IP, data));
        ASSERT_EQ(data, bit(1));
        ASSERT_OK(out.readw<u32>(ADDR_LOW_IP, data));
        ASSERT_EQ(data, bit(1));

        ASSERT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_RISE_IE, bit(1)));
        ASSERT_OK(out.writew<u32>(ADDR_FALL_IE, bit(1)));
        ASSERT_OK(out.writew<u32>(ADDR_HIGH_IE, bit(1)));
        ASSERT_OK(out.writew<u32>(ADDR_LOW_IE, bit(1)));
        ASSERT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_RISE_IP, bit(1)));
        ASSERT_OK(out.readw<u32>(ADDR_RISE_IP, data));
        EXPECT_EQ(data, 0u);
        ASSERT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_FALL_IP, bit(1)));
        ASSERT_OK(out.readw<u32>(ADDR_FALL_IP, data));
        EXPECT_EQ(data, 0u);
        ASSERT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_HIGH_IP, bit(1)));
        ASSERT_OK(out.readw<u32>(ADDR_HIGH_IP, data));
        EXPECT_EQ(data, 0u);
        ASSERT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_LOW_IP, bit(1)));
        ASSERT_OK(out.readw<u32>(ADDR_LOW_IP, data));
        EXPECT_EQ(data, 0u);
        ASSERT_FALSE(irq);

        GTEST_LOG_(INFO) << "test complete";
    }

    void test_commands() {
        GTEST_LOG_(INFO) << "testing commands";

        model.execute("set", { "3" }, std::cout);
        std::cout << std::endl;
        model.execute("status", std::cout);
        model.execute("clear", { "3" }, std::cout);
        std::cout << std::endl;
        model.execute("status", std::cout);

        GTEST_LOG_(INFO) << "test complete";
    }

    virtual void run_test() override {
        wait(SC_ZERO_TIME);
        test_output();
        wait(SC_ZERO_TIME);
        test_input();
        wait(SC_ZERO_TIME);
        test_commands();
    }
};

TEST(gpio, sifive) {
    gpio_sifive_test test("test");
    sc_core::sc_start();
}
