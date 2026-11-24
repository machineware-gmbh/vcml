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
    LM75_ADDR = 0x48,
    REG_TEMP = i2c::lm75::REG_TEMP,
    REG_CONF = i2c::lm75::REG_CONF,
    REG_HYST = i2c::lm75::REG_HYST,
    REG_HIGH = i2c::lm75::REG_HIGH,
    CFG_SHUTDOWN = i2c::lm75::CFG_SHUTDOWN,
    CFG_INT = i2c::lm75::CFG_INT,
};

class lm75_test : public test_base
{
public:
    i2c::lm75 lm75;
    i2c_initiator_socket i2c;
    gpio_target_socket alarm;

    lm75_test(const sc_module_name& nm):
        test_base(nm), lm75("lm75"), i2c("i2c"), alarm("alarm") {
        i2c.bind(lm75.i2c);
        lm75.alarm.bind(alarm);

        add_test("strings", &lm75_test::test_strings);
        add_test("temperature", &lm75_test::test_temperature);
        add_test("thresholds", &lm75_test::test_thresholds);
        add_test("config", &lm75_test::test_config);
        add_test("comparator", &lm75_test::test_comparator);
        add_test("interrupt", &lm75_test::test_interrupt);
        add_test("shutdown", &lm75_test::test_shutdown);
        add_test("long_write", &lm75_test::test_long_write);
        add_test("address", &lm75_test::test_address);
    }

    vector<u8> read(u8 reg, size_t len) {
        vector<u8> data(len);
        EXPECT_EQ(i2c.start(LM75_ADDR, TLM_WRITE_COMMAND), I2C_ACK);
        EXPECT_EQ(i2c.transport(reg), I2C_ACK);
        EXPECT_EQ(i2c.start(LM75_ADDR, TLM_READ_COMMAND), I2C_ACK);
        for (u8& val : data)
            EXPECT_EQ(i2c.transport(val), I2C_ACK);
        EXPECT_EQ(i2c.stop(), I2C_ACK);
        return data;
    }

    void write(u8 reg, vector<u8> data) {
        EXPECT_EQ(i2c.start(LM75_ADDR, TLM_WRITE_COMMAND), I2C_ACK);
        EXPECT_EQ(i2c.transport(reg), I2C_ACK);
        for (u8& val : data)
            EXPECT_EQ(i2c.transport(val), I2C_ACK);
        EXPECT_EQ(i2c.stop(), I2C_ACK);
    }

    double read_temp(u8 reg) {
        vector<u8> data = read(reg, 2);
        EXPECT_EQ(data[1] & 0x7f, 0);
        i16 raw = (i16)(data[0] << 8 | data[1]);
        return raw / 256.0;
    }

    void write_temp(u8 reg, double temp) {
        i16 raw = (i16)(temp * 256.0);
        write(reg, { (u8)(raw >> 8), (u8)(raw & 0x80) });
    }

    void set_temp(double temp) {
        stringstream ss;
        EXPECT_TRUE(lm75.execute("set_temp", { to_string(temp) }, ss));
        wait(sc_time(100, SC_US));
    }

    void restore() {
        write(REG_CONF, { 0 });
        write_temp(REG_HIGH, 80.0);
        write_temp(REG_HYST, 75.0);
        set_temp(22.5);
        read(REG_TEMP, 2);
        EXPECT_FALSE(alarm);
    }

    void test_strings() {
        EXPECT_STREQ(lm75.kind(), "vcml::i2c::lm75");
        EXPECT_STREQ(lm75.version(), VCML_VERSION_STRING);
        EXPECT_EQ(lm75.i2c_addr, LM75_ADDR);
    }

    void test_temperature() {
        EXPECT_EQ(read(REG_TEMP, 2), vector<u8>({ 0x16, 0x80 }));
        EXPECT_EQ(read_temp(REG_TEMP), 22.5);

        for (double temp :
             { 0.0, 0.5, 25.0, 124.5, 125.0, -0.5, -25.0, -55.0 }) {
            set_temp(temp);
            EXPECT_EQ(read_temp(REG_TEMP), temp);
        }

        set_temp(-10.5);
        EXPECT_EQ(read(REG_TEMP, 2), vector<u8>({ 0xf5, 0x80 }));
        EXPECT_EQ(read(REG_TEMP, 4), vector<u8>({ 0xf5, 0x80, 0xff, 0xff }));

        set_temp(200.0);
        EXPECT_EQ(read_temp(REG_TEMP), 125.0);
        set_temp(-100.0);
        EXPECT_EQ(read_temp(REG_TEMP), -55.0);

        write(REG_TEMP, { 0x7f, 0x80 });
        EXPECT_EQ(read_temp(REG_TEMP), -55.0);

        stringstream ss;
        EXPECT_FALSE(lm75.execute("set_temp", { "abc" }, ss));
        EXPECT_FALSE(lm75.execute("set_temp", ss));
        EXPECT_EQ(read_temp(REG_TEMP), -55.0);

        restore();
    }

    void test_thresholds() {
        EXPECT_EQ(read_temp(REG_HIGH), 80.0);
        EXPECT_EQ(read_temp(REG_HYST), 75.0);

        write_temp(REG_HIGH, 30.5);
        write_temp(REG_HYST, -20.0);
        EXPECT_EQ(read_temp(REG_HIGH), 30.5);
        EXPECT_EQ(read_temp(REG_HYST), -20.0);
        EXPECT_EQ(read(REG_HIGH, 2), vector<u8>({ 0x1e, 0x80 }));
        EXPECT_EQ(read(REG_HYST, 2), vector<u8>({ 0xec, 0x00 }));

        stringstream ss;
        EXPECT_TRUE(lm75.execute("set_temp_hi", { "90.5" }, ss));
        EXPECT_TRUE(lm75.execute("set_temp_lo", { "60" }, ss));
        EXPECT_EQ(read_temp(REG_HIGH), 90.5);
        EXPECT_EQ(read_temp(REG_HYST), 60.0);
        EXPECT_FALSE(lm75.execute("set_temp_hi", { "x" }, ss));
        EXPECT_FALSE(lm75.execute("set_temp_lo", { "x" }, ss));
        EXPECT_EQ(read_temp(REG_HIGH), 90.5);
        EXPECT_EQ(read_temp(REG_HYST), 60.0);

        restore();
    }

    void test_config() {
        EXPECT_EQ(read(REG_CONF, 1), vector<u8>({ 0 }));
        write(REG_CONF, { 0x1a });
        EXPECT_EQ(read(REG_CONF, 1), vector<u8>({ 0x1a }));
        EXPECT_EQ(read_temp(REG_HIGH), 80.0);

        EXPECT_EQ(i2c.start(LM75_ADDR, TLM_WRITE_COMMAND), I2C_ACK);
        u8 reg = REG_HYST;
        EXPECT_EQ(i2c.transport(reg), I2C_ACK);
        EXPECT_EQ(i2c.stop(), I2C_ACK);
        EXPECT_EQ(i2c.start(LM75_ADDR, TLM_READ_COMMAND), I2C_ACK);
        u8 data = 0;
        EXPECT_EQ(i2c.transport(data), I2C_ACK);
        EXPECT_EQ(data, 0x4b);
        EXPECT_EQ(i2c.stop(), I2C_ACK);

        restore();
    }

    void test_comparator() {
        set_temp(80.0);
        EXPECT_FALSE(alarm);
        set_temp(80.5);
        EXPECT_TRUE(alarm);

        read(REG_TEMP, 2);
        EXPECT_TRUE(alarm);

        set_temp(77.0);
        EXPECT_TRUE(alarm);
        set_temp(75.0);
        EXPECT_TRUE(alarm);
        set_temp(74.5);
        EXPECT_FALSE(alarm);
        set_temp(77.0);
        EXPECT_FALSE(alarm);

        set_temp(-10.0);
        EXPECT_FALSE(alarm);
        set_temp(-55.0);
        EXPECT_FALSE(alarm);

        write_temp(REG_HIGH, -20.0);
        write_temp(REG_HYST, -30.0);
        set_temp(-25.0);
        EXPECT_FALSE(alarm);
        set_temp(-19.5);
        EXPECT_TRUE(alarm);
        set_temp(-25.0);
        EXPECT_TRUE(alarm);
        set_temp(-30.5);
        EXPECT_FALSE(alarm);

        restore();
    }

    void test_interrupt() {
        write(REG_CONF, { CFG_INT });
        set_temp(85.0);
        EXPECT_TRUE(alarm);

        read(REG_CONF, 1);
        EXPECT_FALSE(alarm);
        wait(sc_time(100, SC_US));
        EXPECT_FALSE(alarm);

        set_temp(77.0);
        EXPECT_FALSE(alarm);
        set_temp(70.0);
        EXPECT_TRUE(alarm);

        read(REG_TEMP, 2);
        EXPECT_FALSE(alarm);
        wait(sc_time(100, SC_US));
        EXPECT_FALSE(alarm);

        set_temp(85.0);
        EXPECT_TRUE(alarm);
        set_temp(70.0);
        read(REG_TEMP, 2);

        restore();
    }

    void test_shutdown() {
        set_temp(90.0);
        EXPECT_TRUE(alarm);

        write(REG_CONF, { CFG_SHUTDOWN });
        EXPECT_FALSE(alarm);
        wait(sc_time(100, SC_US));
        EXPECT_FALSE(alarm);

        write(REG_CONF, { 0 });
        EXPECT_TRUE(alarm);

        restore();
    }

    void test_long_write() {
        write(REG_HIGH, { 0x1e, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0x7f, 0x80 });
        EXPECT_EQ(read_temp(REG_HIGH), 30.5);
        EXPECT_EQ(read_temp(REG_HYST), 75.0);
        EXPECT_EQ(read(REG_CONF, 1), vector<u8>({ 0 }));

        restore();
    }

    void test_address() {
        EXPECT_EQ(i2c.start(LM75_ADDR + 1, TLM_WRITE_COMMAND), I2C_NACK);
        EXPECT_EQ(i2c.start(LM75_ADDR + 1, TLM_READ_COMMAND), I2C_NACK);
        i2c.stop();
        EXPECT_EQ(read_temp(REG_TEMP), 22.5);
    }
};

TEST(i2c, lm75) {
    lm75_test test("test");
    sc_core::sc_start();
}
