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

    i2c_response read_reg(u8 reg, u16& val) {
        i2c_response status;
        status = i2c_out.start(ads1015.i2c_addr, TLM_WRITE_COMMAND);
        if (failed(status))
            return status;

        u8 data = reg;
        status = i2c_out.transport(data);
        if (failed(status))
            return status;

        u16 regval = 0;
        status = i2c_out.start(ads1015.i2c_addr, TLM_READ_COMMAND);
        if (failed(status))
            return status;

        status = i2c_out.transport(data);
        if (failed(status))
            return status;

        regval |= (u16)data << 8;
        status = i2c_out.transport(data);
        if (failed(status))
            return status;

        regval |= data;
        status = i2c_out.stop();
        if (failed(status))
            return status;

        val = regval;
        return I2C_ACK;
    }

    i2c_response write_reg(u8 reg, u16 val) {
        i2c_response status;
        status = i2c_out.start(ads1015.i2c_addr, TLM_WRITE_COMMAND);
        if (failed(status))
            return status;

        u8 data = reg;
        status = i2c_out.transport(data);
        if (failed(status))
            return status;

        data = val >> 8;
        status = i2c_out.transport(data);
        if (failed(status))
            return status;

        data = val & 0xff;
        status = i2c_out.transport(data);
        if (failed(status))
            return status;

        status = i2c_out.stop();
        if (failed(status))
            return status;

        return I2C_ACK;
    }

    ads1015test(const sc_module_name& nm):
        test_base(nm),
        ads1015("ads1015", 0x55),
        i2c_out("i2c_out"),
        alert_in("alert_in") {
        i2c_out.bind(ads1015.i2c_in);
        ads1015.alert.bind(alert_in);
        add_test("strings", &ads1015test::test_strings);
        add_test("read_regs", &ads1015test::test_read_regs);
        add_test("read_voltage", &ads1015test::test_read_voltage);
        add_test("commands_get", &ads1015test::test_commands_get);
        add_test("commands_set", &ads1015test::test_commands_set);
    }

    void test_strings() {
        EXPECT_STREQ(ads1015.kind(), "vcml::i2c::ads1015");
        EXPECT_STREQ(ads1015.version(), VCML_VERSION_STRING);
    }

    void test_read_regs() {
        u16 config, thresh_lo, thresh_hi;
        ASSERT_EQ(read_reg(1, config), I2C_ACK);
        ASSERT_EQ(read_reg(2, thresh_lo), I2C_ACK);
        ASSERT_EQ(read_reg(3, thresh_hi), I2C_ACK);

        EXPECT_EQ(config, 0x8583);
        EXPECT_EQ(thresh_lo, 0x8000);
        EXPECT_EQ(thresh_hi, 0x7fff);
    }

    void test_read_voltage() {
        ads1015.ain0 = 1.5;

        u16 channel = 4; // ain0
        u16 pga_fsr = 2; // +-2.048V
        u16 mode = 1;    // single-shot
        u16 rate = 4;    // 1600SPS
        u16 comp = 3;    // comp off

        u16 control = 1u << 15 | channel << 12 | pga_fsr << 9 | mode << 8 |
                      rate << 6 | comp;

        u16 voltage = 0;
        ASSERT_EQ(write_reg(1, control), I2C_ACK);
        ASSERT_EQ(read_reg(0, voltage), I2C_ACK);

        u16 expect = ads1015.ain0 * 1000.0; // mV
        EXPECT_EQ(voltage, expect << 4);
    }

    void test_commands_get() {
        stringstream ss;
        string output = "ain0: 1.500\nain1: 1.000\nain2: 1.300\nain3: -0.700";
        EXPECT_TRUE(ads1015.execute("get_voltage", ss));
        EXPECT_EQ(ss.str(), output);

        ss.str("");
        EXPECT_TRUE(ads1015.execute("get_voltage", { "ain0" }, ss));
        EXPECT_EQ(ss.str(), "ain0: 1.500");

        ss.str("");
        EXPECT_TRUE(ads1015.execute("get_voltage", { "ain1" }, ss));
        EXPECT_EQ(ss.str(), "ain1: 1.000");

        ss.str("");
        EXPECT_TRUE(ads1015.execute("get_voltage", { "ain2" }, ss));
        EXPECT_EQ(ss.str(), "ain2: 1.300");

        ss.str("");
        EXPECT_TRUE(ads1015.execute("get_voltage", { "ain3" }, ss));
        EXPECT_EQ(ss.str(), "ain3: -0.700");

        ss.str("");
        output = "unknown channel: xxx\nuse: ain0, ain1, ain2, or ain3";
        EXPECT_FALSE(ads1015.execute("get_voltage", { "xxx" }, ss));
        EXPECT_EQ(ss.str(), output);
    }

    void test_commands_set() {
        stringstream ss;
        EXPECT_TRUE(ads1015.execute("set_voltage", { "ain0", "0.1" }, ss));
        EXPECT_TRUE(ads1015.execute("set_voltage", { "ain1", "0.2" }, ss));
        EXPECT_TRUE(ads1015.execute("set_voltage", { "ain2", "0.3" }, ss));
        EXPECT_TRUE(ads1015.execute("set_voltage", { "ain3", "-0.4" }, ss));

        ss.str("");
        string output = "unknown channel: xxx\nuse: ain0, ain1, ain2, or ain3";
        EXPECT_FALSE(ads1015.execute("set_voltage", { "xxx", "0.0" }, ss));
        EXPECT_EQ(ss.str(), output);

        ss.str("");
        output = "ain0: 0.100\nain1: 0.200\nain2: 0.300\nain3: -0.400";
        EXPECT_TRUE(ads1015.execute("get_voltage", ss));
        EXPECT_EQ(ss.str(), output);
    }
};

TEST(i2c, ads1015) {
    ads1015test test("test");
    sc_core::sc_start();
}
