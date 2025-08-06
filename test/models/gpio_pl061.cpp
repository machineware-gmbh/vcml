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

class pl061test : public test_base
{
public:
    gpio::pl061 pl061;

    tlm_initiator_socket out;
    gpio_initiator_socket gpio1;
    gpio_target_socket gpio3;
    gpio_target_socket irq;

    pl061test(const sc_module_name& nm):
        test_base(nm),
        pl061("pl061"),
        out("out"),
        gpio1("gpio1"),
        gpio3("gpio3"),
        irq("irq") {
        out.bind(pl061.in);
        clk.bind(pl061.clk);
        rst.bind(pl061.rst);

        pl061.gpio_out[3].bind(gpio3);
        gpio1.bind(pl061.gpio_in[1]);
        pl061.intr.bind(irq);

        add_test("strings", &pl061test::test_strings);
        add_test("output", &pl061test::test_output);
        add_test("input", &pl061test::test_input);
        add_test("interrupts", &pl061test::test_interrupts);
        add_test("commands", &pl061test::test_commands);
    }

    void test_strings() {
        EXPECT_STREQ(pl061.kind(), "vcml::gpio::pl061");
        EXPECT_STREQ(pl061.version(), VCML_VERSION_STRING);
    }

    enum : u64 {
        ADDR_DATA = 0x000,
        ADDR_DIR = 0x400,
        ADDR_IS = 0x404,
        ADDR_IBE = 0x408,
        ADDR_IEV = 0x40c,
        ADDR_IE = 0x410,
        ADDR_RIS = 0x414,
        ADDR_MIS = 0x418,
        ADDR_IC = 0x41c,
        ADDR_AFSEL = 0x420,
    };

    void test_output() {
        EXPECT_FALSE(gpio3);
        ASSERT_OK(out.writew<u8>(ADDR_DIR, 1 << 3)); // gpio3 output

        u8 mask = 1u << 3;
        u64 addr = ADDR_DATA + ((u64)mask << 2);
        ASSERT_OK(out.writew<u8>(addr, mask));
        EXPECT_TRUE(gpio3);
        ASSERT_OK(out.writew<u8>(addr, 0));
        EXPECT_FALSE(gpio3);
    }

    void test_input() {
        u32 dir = 0;
        ASSERT_OK(out.readw<u32>(ADDR_DIR, dir));
        dir &= ~(1u << 1);
        ASSERT_OK(out.writew<u32>(ADDR_DIR, dir));

        u32 data = 0;
        u64 addr = ADDR_DATA + (1u << (1 + 2));
        ASSERT_OK(out.readw<u32>(addr, data));
        EXPECT_EQ(data, 0u);

        gpio1 = true;
        ASSERT_OK(out.readw<u32>(addr, data));
        EXPECT_EQ(data, 1u << 1);

        gpio1 = false;
        ASSERT_OK(out.readw<u32>(addr, data));
        EXPECT_EQ(data, 0u);
    }

    void test_interrupts() {
        u32 dir = 0;
        ASSERT_OK(out.readw<u32>(ADDR_DIR, dir));
        dir &= ~(1u << 1);
        ASSERT_OK(out.writew<u32>(ADDR_DIR, dir));     // gpio1 input
        ASSERT_OK(out.writew<u32>(ADDR_IS, 1u << 1));  // level irq
        ASSERT_OK(out.writew<u32>(ADDR_IEV, 1u << 1)); // level high

        gpio1 = true;
        EXPECT_FALSE(irq);

        u32 ris = 0, mis = 0;
        ASSERT_OK(out.readw<u32>(ADDR_RIS, ris)); // raw irq state
        EXPECT_EQ(ris, 1u << 1);
        ASSERT_OK(out.readw<u32>(ADDR_MIS, mis)); // masked irq state
        EXPECT_EQ(mis, 0u);

        ASSERT_OK(out.writew<u32>(ADDR_IE, 1u << 1)); // enable irq1
        EXPECT_TRUE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_IE, 0u)); // disable irq1
        EXPECT_FALSE(irq);
        ASSERT_OK(out.writew<u32>(ADDR_IE, 1u << 1)); // enable irq1
        ASSERT_OK(out.writew<u32>(ADDR_IEV, 0u));     // level low
        EXPECT_FALSE(irq);
        gpio1 = false;
        EXPECT_TRUE(irq);
    }

    void test_commands() {
        stringstream ss;
        pl061.execute("set", { "3" }, ss);
        EXPECT_EQ(ss.str(), "GPIO3 set");

        ss.str("");
        pl061.execute("status", ss);
        EXPECT_EQ(ss.str(),
                  "GPIO0 not connected\n"
                  "GPIO1 input is low\n"
                  "GPIO2 not connected\n"
                  "GPIO3 output is high\n"
                  "GPIO4 not connected\n"
                  "GPIO5 not connected\n"
                  "GPIO6 not connected\n"
                  "GPIO7 not connected\n"
                  "  RIS 00000000\n"
                  "  MIS 00000000");

        ss.str("");
        pl061.execute("clear", { "3" }, ss);
        EXPECT_EQ(ss.str(), "GPIO3 cleared");

        ss.str("");
        pl061.execute("status", ss);
        EXPECT_EQ(ss.str(),
                  "GPIO0 not connected\n"
                  "GPIO1 input is low\n"
                  "GPIO2 not connected\n"
                  "GPIO3 output is low\n"
                  "GPIO4 not connected\n"
                  "GPIO5 not connected\n"
                  "GPIO6 not connected\n"
                  "GPIO7 not connected\n"
                  "  RIS 00001000\n"
                  "  MIS 00000000");
        std::cout << ss.str() << std::endl;
    }
};

TEST(gpio, sifive) {
    pl061test test("test");
    sc_core::sc_start();
}
