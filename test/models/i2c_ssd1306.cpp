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

struct ssd1306_test : public test_base, i2c_host {
    static constexpr u8 SSD1306_ADDR = 0x3cu;
    static constexpr u8 CONTROL_MULTI_CMD = 0x00u;
    static constexpr u8 CONTROL_MULTI_DATA = 0x40u;
    static constexpr u8 CONTROL_SINGLE_CMD = 0x80u;
    static constexpr u8 CONTROL_SINGLE_DATA = 0xc0u;
    static constexpr size_t SCREEN_WIDTH = 128;
    static constexpr size_t SCREEN_HEIGHT = 64;

    i2c::ssd1306 screen;
    i2c_initiator_socket socket;
    i2c_response resp;
    u8 data;

    ssd1306_test(const sc_module_name& nm):
        test_base(nm), screen("screen"), socket("socket") {
        socket.bind(screen.in);
        add_test("init_state", &ssd1306_test::test_init_state);
        add_test("address", &ssd1306_test::test_address);
        add_test("on_off", &ssd1306_test::test_on_off);
        add_test("contrast", &ssd1306_test::test_contrast);
        add_test("inverse", &ssd1306_test::test_inverse);
        add_test("read_status", &ssd1306_test::test_read_status);
        add_test("write_gddram", &ssd1306_test::test_write_gddram);
        add_test("nop", &ssd1306_test::test_nop);
        add_test("entire_display_on", &ssd1306_test::test_entire_display_on);

        screen.rotated = false;
        rst.bind(screen.rst);
    }

    void test_init_state() {
        for (size_t y = 0; y < SCREEN_HEIGHT; y++)
            for (size_t x = 0; x < SCREEN_WIDTH; x++)
                ASSERT_FALSE(screen.read_pixel(x, y));

        ASSERT_FALSE(screen.is_display_on());
        ASSERT_FALSE(screen.is_inverse());
        ASSERT_EQ(screen.get_contrast(), 0x7fu);
    }

    void test_address() {
        i2c_response resp;

        resp = socket.start(0x23, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_NACK);

        resp = socket.start(I2C_ADDR_BCAST, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);

        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_on_off() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_SINGLE_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xaf;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_TRUE(screen.is_display_on());

        data = 0xaf;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_TRUE(screen.is_display_on());

        data = 0xae;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_FALSE(screen.is_display_on());

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_contrast() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_MULTI_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0x81;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0x12;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_EQ(screen.get_contrast(), 0x12);

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_inverse() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_MULTI_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xa7;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_TRUE(screen.is_inverse());
        ASSERT_FALSE(screen.read_pixel(0, 0));

        data = 0xaf;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_TRUE(screen.is_inverse());
        ASSERT_TRUE(screen.read_pixel(0, 0));

        data = 0xa6;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_FALSE(screen.is_inverse());
        ASSERT_FALSE(screen.read_pixel(0, 0));

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_read_status() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_READ_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_MULTI_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_EQ(data, 0x40);

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_write_gddram() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_SINGLE_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xaf;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        ASSERT_TRUE(screen.is_display_on());

        data = CONTROL_MULTI_DATA;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xff;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(0, i));

        ASSERT_FALSE(screen.read_pixel(0, 8));
        ASSERT_FALSE(screen.read_pixel(1, 0));

        data = 0xff;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(1, i));

        ASSERT_FALSE(screen.read_pixel(1, 8));
        ASSERT_FALSE(screen.read_pixel(2, 0));

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_nop() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_MULTI_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xe3;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void test_entire_display_on() {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_MULTI_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xaf;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        data = 0xa5;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        for (size_t y = 0; y < SCREEN_HEIGHT; y++)
            for (size_t x = 0; x < SCREEN_WIDTH; x++)
                ASSERT_TRUE(screen.read_pixel(x, y));

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }
};

TEST(i2c, ssd1306_test) {
    ssd1306_test test("test");
    sc_core::sc_start();
}
