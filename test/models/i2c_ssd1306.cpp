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
        add_test("scroll_default_off", &ssd1306_test::test_scroll_default_off);
        add_test("scroll_activate_deactivate",
                 &ssd1306_test::test_scroll_activate_deactivate);
        add_test("horizontal_scroll_right",
                 &ssd1306_test::test_horizontal_scroll_right);
        add_test("horizontal_scroll_left",
                 &ssd1306_test::test_horizontal_scroll_left);
        add_test("horizontal_scroll_page_range",
                 &ssd1306_test::test_horizontal_scroll_page_range);
        add_test("vertical_scroll_area",
                 &ssd1306_test::test_vertical_scroll_area);
        add_test("scroll_setup_split_across_transactions",
                 &ssd1306_test::test_scroll_setup_split_across_transactions);

        rst.bind(screen.rst);
    }

    void send_commands(std::initializer_list<u8> bytes) {
        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_MULTI_CMD;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        for (u8 b : bytes) {
            data = b;
            resp = socket.transport(data);
            ASSERT_EQ(resp, I2C_ACK);
        }

        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
    }

    void send_commands_split(std::initializer_list<u8> bytes) {
        for (u8 b : bytes) {
            resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
            ASSERT_EQ(resp, I2C_ACK);
            data = CONTROL_SINGLE_CMD;
            resp = socket.transport(data);
            ASSERT_EQ(resp, I2C_ACK);
            data = b;
            resp = socket.transport(data);
            ASSERT_EQ(resp, I2C_ACK);
            resp = socket.stop();
            ASSERT_EQ(resp, I2C_ACK);
        }
    }

    void write_byte(u8 page, u8 col, u8 value) {
        send_commands({ (u8)(0xb0 | (page & 0x7)), (u8)(0x00 | (col & 0x0f)),
                        (u8)(0x10 | ((col >> 4) & 0x0f)) });

        resp = socket.start(SSD1306_ADDR, tlm::TLM_WRITE_COMMAND);
        ASSERT_EQ(resp, I2C_ACK);
        data = CONTROL_SINGLE_DATA;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        data = value;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);
        resp = socket.stop();
        ASSERT_EQ(resp, I2C_ACK);
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
            ASSERT_TRUE(screen.read_pixel(127, 63 - i));

        ASSERT_FALSE(screen.read_pixel(127, 55));
        ASSERT_FALSE(screen.read_pixel(126, 63));

        data = 0xff;
        resp = socket.transport(data);
        ASSERT_EQ(resp, I2C_ACK);

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(126, 63 - i));

        ASSERT_FALSE(screen.read_pixel(126, 55));
        ASSERT_FALSE(screen.read_pixel(125, 63));

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

    void test_scroll_default_off() {
        screen.reset();

        ASSERT_FALSE(screen.is_scrolling());
        ASSERT_GT(screen.frame_period().to_seconds(), 0.0);
    }

    void test_scroll_activate_deactivate() {
        screen.reset();

        send_commands({ 0x2e });
        ASSERT_FALSE(screen.is_scrolling());

        send_commands({ 0x26, 0x00, 0x00, 0x07, 0x07, 0x00, 0xff });
        ASSERT_FALSE(screen.is_scrolling());

        send_commands({ 0x2f });
        ASSERT_TRUE(screen.is_scrolling());

        send_commands({ 0x2e });
        ASSERT_FALSE(screen.is_scrolling());
    }

    void test_horizontal_scroll_right() {
        screen.reset();
        send_commands({ 0xaf });
        send_commands({ 0xa4 });
        write_byte(0, 0, 0xff);

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(127, 63 - i));

        send_commands({ 0x26, 0x00, 0x00, 0x07, 0x07, 0x00, 0xff });
        send_commands({ 0x2f });
        ASSERT_TRUE(screen.is_scrolling());

        wait(screen.frame_period() * 2.0);
        wait(sc_core::SC_ZERO_TIME);

        for (size_t i = 0; i < 8; i++) {
            ASSERT_FALSE(screen.read_pixel(127, 63 - i));
            ASSERT_TRUE(screen.read_pixel(126, 63 - i));
        }
    }

    void test_horizontal_scroll_left() {
        screen.reset();
        send_commands({ 0xaf });
        send_commands({ 0xa4 });
        write_byte(0, 0, 0xff);

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(127, 63 - i));

        send_commands({ 0x27, 0x00, 0x00, 0x07, 0x07, 0x00, 0xff });
        send_commands({ 0x2f });

        wait(screen.frame_period() * 2.0);
        wait(sc_core::SC_ZERO_TIME);

        for (size_t i = 0; i < 8; i++) {
            ASSERT_FALSE(screen.read_pixel(127, 63 - i));
            ASSERT_TRUE(screen.read_pixel(0, 63 - i));
        }
    }

    void test_horizontal_scroll_page_range() {
        screen.reset();
        send_commands({ 0xaf });
        send_commands({ 0xa4 });
        write_byte(0, 0, 0xff);
        write_byte(1, 0, 0xff);

        send_commands({ 0x26, 0x00, 0x00, 0x07, 0x00, 0x00, 0xff });
        send_commands({ 0x2f });

        wait(screen.frame_period() * 2.0);
        wait(sc_core::SC_ZERO_TIME);

        for (size_t i = 0; i < 8; i++) {
            ASSERT_FALSE(screen.read_pixel(127, 63 - i));
            ASSERT_TRUE(screen.read_pixel(126, 63 - i));
        }

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(127, 55 - i));
    }

    void test_vertical_scroll_area() {
        screen.reset();
        send_commands({ 0xaf });
        send_commands({ 0xa4 });
        write_byte(0, 5, 0x01);

        ASSERT_TRUE(screen.read_pixel(122, 63));
        for (size_t y = 56; y <= 62; y++)
            ASSERT_FALSE(screen.read_pixel(122, y));

        send_commands({ 0xa3, 0x00, 0x08 });
        send_commands({ 0x29, 0x00, 0x01, 0x07, 0x01, 0x01 });
        send_commands({ 0x2f });
        ASSERT_TRUE(screen.is_scrolling());

        wait(screen.frame_period() * 2.0);
        wait(sc_core::SC_ZERO_TIME);

        ASSERT_FALSE(screen.read_pixel(122, 63));
        ASSERT_TRUE(screen.read_pixel(122, 56));
        for (size_t y = 57; y <= 62; y++)
            ASSERT_FALSE(screen.read_pixel(122, y));
    }

    void test_scroll_setup_split_across_transactions() {
        screen.reset();
        send_commands({ 0xaf });
        send_commands({ 0xa4 });
        write_byte(0, 0, 0xff);

        for (size_t i = 0; i < 8; i++)
            ASSERT_TRUE(screen.read_pixel(127, 63 - i));

        send_commands({ 0x26, 0x00 });
        send_commands_split({ 0x00 });
        send_commands_split({ 0x00 });
        send_commands_split({ 0x07 });
        send_commands({ 0x00, 0xff });
        send_commands_split({ 0x2f });

        ASSERT_TRUE(screen.is_scrolling());

        wait(screen.frame_period() * 5.0);
        wait(sc_core::SC_ZERO_TIME);

        for (size_t i = 0; i < 8; i++) {
            ASSERT_FALSE(screen.read_pixel(127, 63 - i));
            ASSERT_TRUE(screen.read_pixel(126, 63 - i));
        }
    }
};

TEST(i2c, ssd1306_test) {
    ssd1306_test test("test");
    sc_core::sc_start();
}
