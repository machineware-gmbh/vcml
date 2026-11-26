/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "usb_test.h"

enum hid_requests : u16 {
    HID_GET_REPORT = USB_REQ_CLASS | USB_REQ_IFACE | 0x01,
    HID_GET_IDLE = USB_REQ_CLASS | USB_REQ_IFACE | 0x02,
    HID_GET_PROTOCOL = USB_REQ_CLASS | USB_REQ_IFACE | 0x03,
    HID_SET_REPORT = USB_REQ_CLASS | USB_REQ_IFACE | 0x09,
    HID_SET_IDLE = USB_REQ_CLASS | USB_REQ_IFACE | 0x0a,
    HID_SET_PROTOCOL = USB_REQ_CLASS | USB_REQ_IFACE | 0x0b,
};

enum hid_usages : u8 {
    HID_ROLLOVER = 0x01,
    HID_A = 0x04,
    HID_B = 0x05,
    HID_C = 0x06,
    HID_D = 0x07,
    HID_E = 0x08,
    HID_F = 0x09,
    HID_G = 0x0a,
    HID_1 = 0x1e,
    HID_CAPSLOCK = 0x39,
};

static vector<u8> hid_report(u8 mods, const vector<u8>& keys = {}) {
    vector<u8> r(8, 0);
    r[0] = mods;
    for (size_t i = 0; i < keys.size() && i < 6; i++)
        r[2 + i] = keys[i];
    return r;
}

class usb_keyboard_test : public test_base
{
public:
    usb_stim stim;
    usb_stim stim2;
    usb::keyboard kbd;
    usb::keyboard kbd2;

    usb_keyboard_test(const sc_module_name& nm):
        test_base(nm), stim("stim"), stim2("stim2"), kbd("kbd"), kbd2("kbd2") {
        stim.usb_out.bind(kbd.usb_in);
        stim2.usb_out.bind(kbd2.usb_in);

        add_test("strings", &usb_keyboard_test::test_strings);
        add_test("properties", &usb_keyboard_test::test_properties);
        add_test("device_desc", &usb_keyboard_test::test_device_desc);
        add_test("config_desc", &usb_keyboard_test::test_config_desc);
        add_test("report_desc", &usb_keyboard_test::test_report_desc);
        add_test("string_desc", &usb_keyboard_test::test_string_desc);
        add_test("hid_requests", &usb_keyboard_test::test_hid_requests);
        add_test("leds", &usb_keyboard_test::test_leds);
        add_test("keys", &usb_keyboard_test::test_keys);
        add_test("modifiers", &usb_keyboard_test::test_modifiers);
        add_test("shifted", &usb_keyboard_test::test_shifted);
        add_test("rollover", &usb_keyboard_test::test_rollover);
        add_test("unmapped", &usb_keyboard_test::test_unmapped);
        add_test("unconfigured", &usb_keyboard_test::test_unconfigured);
        add_test("endpoints", &usb_keyboard_test::test_endpoints);
    }

    ui::keyboard& input(usb::keyboard& dev) {
        string nm = mkstr("%s.keyboard", dev.name());
        ui::keyboard* kb = ui::input::find<ui::keyboard>(nm);
        VCML_ERROR_ON(!kb, "keyboard %s not found", nm.c_str());
        return *kb;
    }

    void enumerate(usb_stim& host, u32 addr) {
        host.usb_out.reset_device();
        host.addr = 0;
        EXPECT_SUCCESS(host.set_address(addr));
        EXPECT_SUCCESS(host.set_configuration(42));
    }

    vector<u8> poll(usb_stim& host) {
        vector<u8> data;
        EXPECT_SUCCESS(host.in(1, data, 8));
        return data;
    }

    vector<u8> settle(usb_stim& host, usb::keyboard& dev) {
        vector<u8> data = poll(host);
        while (input(dev).has_events())
            data = poll(host);
        return data;
    }

    vector<u8> press(u32 sym) {
        input(kbd).notify_key(sym, true);
        return settle(stim, kbd);
    }

    vector<u8> release(u32 sym) {
        input(kbd).notify_key(sym, false);
        return settle(stim, kbd);
    }

    void test_strings() {
        EXPECT_STREQ(kbd.kind(), "vcml::usb::keyboard");
        EXPECT_STREQ(kbd.version(), VCML_VERSION_STRING);
    }

    void test_properties() {
        EXPECT_TRUE(kbd.usb3);
        EXPECT_EQ(kbd.vendorid, USB_VENDOR_VCML);
        EXPECT_EQ(kbd.productid, 1);
        EXPECT_EQ(kbd.manufacturer.get(), "MachineWare GmbH");
        EXPECT_EQ(kbd.product.get(), "VCML Keyboard");
        EXPECT_EQ(kbd.serialno.get(), VCML_GIT_REV_SHORT);
        EXPECT_EQ(kbd.keymap.get(), "us");
        EXPECT_STREQ(input(kbd).layout(), "us");

        EXPECT_FALSE(kbd2.usb3);
        EXPECT_EQ(kbd2.vendorid, 0xabcd);
        EXPECT_EQ(kbd2.productid, 0x1234);
        EXPECT_EQ(kbd2.keymap.get(), "");
        EXPECT_STREQ(input(kbd2).layout(), "");

        EXPECT_EQ(stim.attached, 1);
        EXPECT_EQ(stim.detached, 0);
        EXPECT_EQ(stim.usb_out.connection_speed(), USB_SPEED_SUPER);
        EXPECT_EQ(stim2.attached, 1);
        EXPECT_EQ(stim2.usb_out.connection_speed(), USB_SPEED_HIGH);
    }

    void test_device_desc() {
        vector<u8> data;
        enumerate(stim, 1);
        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18));
        EXPECT_EQ(data,
                  vector<u8>({ 18, USB_DT_DEVICE, 0x00, 0x03, 0, 0, 0, 9, 0x37,
                               0x13, 0x01, 0x00, 0x00, 0x00, 1, 2, 3, 1 }));

        enumerate(stim2, 2);
        EXPECT_SUCCESS(stim2.get_descriptor(USB_DT_DEVICE, 0, data, 18));
        EXPECT_EQ(data,
                  vector<u8>({ 18, USB_DT_DEVICE, 0x00, 0x02, 0, 0, 0, 8, 0xcd,
                               0xab, 0x34, 0x12, 0x00, 0x00, 1, 2, 3, 1 }));
    }

    void check_config_desc(usb_stim& host, bool usb3) {
        vector<u8> data;
        size_t total = usb3 ? 40 : 34;
        EXPECT_SUCCESS(host.get_descriptor(USB_DT_CONFIG, 0, data, 9));
        EXPECT_EQ((size_t)(data[2] | data[3] << 8), total);

        EXPECT_SUCCESS(host.get_descriptor(USB_DT_CONFIG, 0, data, total));
        const u8 bytes[] = {
            9,
            USB_DT_CONFIG,
            (u8)total,
            0,
            1,
            42,
            0,
            0xa0,
            40,
            9,
            USB_DT_INTERFACE,
            0,
            0,
            1,
            USB_CLASS_HID,
            1,
            1,
            0,
            9,
            USB_DT_HID,
            0x11,
            0x01,
            0,
            1,
            USB_DT_REPORT,
            63,
            0,
            7,
            USB_DT_ENDPOINT,
            0x81,
            USB_EP_IRQ,
            8,
            0,
            8,
            6,
            USB_DT_ENDPOINT_COMPANION,
            0,
            0,
            8,
            0,
        };

        ASSERT_LE(total, sizeof(bytes));
        EXPECT_EQ(data, vector<u8>(bytes, bytes + total));

        EXPECT_FAILURE(host.get_descriptor(USB_DT_CONFIG, 1, data, 9));
    }

    void test_config_desc() {
        enumerate(stim, 1);
        check_config_desc(stim, true);
        enumerate(stim2, 2);
        check_config_desc(stim2, false);
    }

    void test_report_desc() {
        vector<u8> cfg, data;
        enumerate(stim, 1);
        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_CONFIG, 0, cfg, 27));
        size_t len = cfg[25] | cfg[26] << 8;
        ASSERT_EQ(len, 63);

        EXPECT_SUCCESS(
            stim.get_descriptor(USB_DT_REPORT, 0, data, len, USB_REQ_IFACE));
        ASSERT_EQ(data.size(), len);
        EXPECT_EQ(vector<u8>(data.begin(), data.begin() + 6),
                  vector<u8>({ 0x05, 0x01, 0x09, 0x06, 0xa1, 0x01 }));
        EXPECT_EQ(data.back(), 0xc0);

        size_t rsize = 0, rcount = 0, inbits = 0, outbits = 0;
        int depth = 0;
        size_t pos = 0;
        while (pos < data.size()) {
            u8 prefix = data[pos];
            size_t n = (prefix & 3) == 3 ? 4 : prefix & 3;
            ASSERT_LE(pos + 1 + n, data.size());
            u32 val = 0;
            for (size_t i = 0; i < n; i++)
                val |= (u32)data[pos + 1 + i] << (8 * i);

            switch (prefix & 0xfc) {
            case 0x74:
                rsize = val;
                break;
            case 0x94:
                rcount = val;
                break;
            case 0x80:
                inbits += rsize * rcount;
                break;
            case 0x90:
                outbits += rsize * rcount;
                break;
            case 0xa0:
                depth++;
                break;
            case 0xc0:
                depth--;
                break;
            default:
                break;
            }

            pos += 1 + n;
        }

        EXPECT_EQ(pos, data.size());
        EXPECT_EQ(depth, 0);
        EXPECT_EQ(inbits, 64);
        EXPECT_EQ(outbits, 8);

        EXPECT_FAILURE(stim.get_descriptor(0x23, 0, data, 8, USB_REQ_IFACE));
        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_DEVICE, 0, data, 8));
    }

    void test_string_desc() {
        vector<u8> data;
        enumerate(stim, 1);
        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_STRING, 0, data, 4));
        EXPECT_EQ(data, vector<u8>({ 4, USB_DT_STRING, 0x09, 0x04 }));
        EXPECT_EQ(stim.get_string(1), "MachineWare GmbH");
        EXPECT_EQ(stim.get_string(2), "VCML Keyboard");
        EXPECT_EQ(stim.get_string(3), VCML_GIT_REV_SHORT);

        enumerate(stim2, 2);
        EXPECT_EQ(stim2.get_string(1), "Test Vendor");
        EXPECT_EQ(stim2.get_string(2), "Test Keyboard");
        EXPECT_EQ(stim2.get_string(3), "12345");
    }

    void test_hid_requests() {
        vector<u8> data;
        enumerate(stim, 1);

        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_CONFIG, 0, data, 40));
        EXPECT_SUCCESS(stim.control_in(HID_GET_REPORT, 0x0100, 0, data, 8));
        EXPECT_EQ(data, hid_report(0));

        input(kbd).notify_key(ui::KEYSYM_a, true);
        EXPECT_SUCCESS(stim.control_in(HID_GET_REPORT, 0x0100, 0, data, 8));
        EXPECT_EQ(data, hid_report(0, { HID_A }));
        EXPECT_EQ(release(ui::KEYSYM_a), hid_report(0));

        EXPECT_FAILURE(stim.control_in(HID_GET_REPORT, 0x0100, 0, data, 4));

        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_CONFIG, 0, data, 40));
        EXPECT_SUCCESS(stim.control_in(HID_GET_IDLE, 0, 0, data, 1));
        EXPECT_EQ(data, vector<u8>({ 0 }));
        EXPECT_SUCCESS(stim.control_out(HID_SET_IDLE, 0, 0));

        EXPECT_SUCCESS(stim.control_in(HID_GET_PROTOCOL, 0, 0, data, 1));
        EXPECT_EQ(data, vector<u8>({ 1 }));
        EXPECT_SUCCESS(stim.control_out(HID_SET_PROTOCOL, 1, 0));

        EXPECT_FAILURE(stim.control_in(USB_REQ_CLASS | USB_REQ_IFACE | 0x05, 0,
                                       0, data, 1));
        EXPECT_FAILURE(
            stim.control_out(USB_REQ_CLASS | USB_REQ_IFACE | 0x05, 0, 0));
        EXPECT_FAILURE(stim.control_in(USB_REQ_CLASS | USB_REQ_ENDPOINT | 0x01,
                                       0, 0, data, 8));
        EXPECT_SUCCESS(stim.get_descriptor(USB_DT_DEVICE, 0, data, 8));
    }

    void test_leds() {
        using led = usb::keyboard::led_type;
        const led all[] = { usb::keyboard::LED_NUM_LOCK,
                            usb::keyboard::LED_CAPS_LOCK,
                            usb::keyboard::LED_SCROLL_LOCK,
                            usb::keyboard::LED_COMPOSE,
                            usb::keyboard::LED_KANA };

        enumerate(stim, 1);
        for (led l : all)
            EXPECT_FALSE(kbd.get_led(l));

        EXPECT_SUCCESS(stim.control_out(HID_SET_REPORT, 0x0200, 0, { 0x03 }));
        EXPECT_TRUE(kbd.get_led(usb::keyboard::LED_NUM_LOCK));
        EXPECT_TRUE(kbd.get_led(usb::keyboard::LED_CAPS_LOCK));
        EXPECT_FALSE(kbd.get_led(usb::keyboard::LED_SCROLL_LOCK));

        EXPECT_SUCCESS(stim.control_out(HID_SET_REPORT, 0x0200, 0, { 0x1f }));
        for (led l : all)
            EXPECT_TRUE(kbd.get_led(l));

        EXPECT_FAILURE(stim.control_out(HID_SET_REPORT, 0x0200, 0, { 0, 0 }));
        for (led l : all)
            EXPECT_TRUE(kbd.get_led(l));

        EXPECT_FAILURE(stim.control_out(HID_SET_REPORT, 0x0200, 0, {}));

        EXPECT_SUCCESS(stim.control_out(HID_SET_REPORT, 0x0200, 0, { 0x04 }));
        EXPECT_FALSE(kbd.get_led(usb::keyboard::LED_NUM_LOCK));
        EXPECT_FALSE(kbd.get_led(usb::keyboard::LED_CAPS_LOCK));
        EXPECT_TRUE(kbd.get_led(usb::keyboard::LED_SCROLL_LOCK));
        EXPECT_FALSE(kbd.get_led(usb::keyboard::LED_COMPOSE));
        EXPECT_FALSE(kbd.get_led(usb::keyboard::LED_KANA));

        EXPECT_SUCCESS(stim.control_out(HID_SET_REPORT, 0x0200, 0, { 0 }));
        for (led l : all)
            EXPECT_FALSE(kbd.get_led(l));
    }

    void test_keys() {
        enumerate(stim, 1);
        EXPECT_EQ(poll(stim), hid_report(0));
        EXPECT_EQ(poll(stim), hid_report(0));

        input(kbd).notify_key(ui::KEYSYM_a, true);
        EXPECT_EQ(poll(stim), hid_report(0, { HID_A }));
        EXPECT_EQ(poll(stim), hid_report(0, { HID_A }));
        EXPECT_FALSE(input(kbd).has_events());

        input(kbd).notify_key(ui::KEYSYM_a, true);
        EXPECT_EQ(settle(stim, kbd), hid_report(0, { HID_A }));

        EXPECT_EQ(press(ui::KEYSYM_b), hid_report(0, { HID_A, HID_B }));
        EXPECT_EQ(press(ui::KEYSYM_1), hid_report(0, { HID_A, HID_B, HID_1 }));
        EXPECT_EQ(release(ui::KEYSYM_b), hid_report(0, { HID_A, HID_1 }));
        EXPECT_EQ(press(ui::KEYSYM_c), hid_report(0, { HID_A, HID_1, HID_C }));
        EXPECT_EQ(release(ui::KEYSYM_a), hid_report(0, { HID_1, HID_C }));
        EXPECT_EQ(release(ui::KEYSYM_c), hid_report(0, { HID_1 }));
        EXPECT_EQ(release(ui::KEYSYM_1), hid_report(0));

        input(kbd).notify_key(ui::KEYSYM_a, true);
        input(kbd).notify_key(ui::KEYSYM_a, false);
        input(kbd).notify_key(ui::KEYSYM_b, true);
        EXPECT_EQ(poll(stim), hid_report(0, { HID_A }));
        EXPECT_EQ(poll(stim), hid_report(0, { HID_A }));
        EXPECT_EQ(poll(stim), hid_report(0));
        EXPECT_EQ(poll(stim), hid_report(0));
        EXPECT_EQ(poll(stim), hid_report(0, { HID_B }));
        EXPECT_EQ(release(ui::KEYSYM_b), hid_report(0));

        EXPECT_EQ(press(ui::KEYSYM_CAPSLOCK), hid_report(0, { HID_CAPSLOCK }));
        EXPECT_EQ(release(ui::KEYSYM_CAPSLOCK), hid_report(0));
        EXPECT_EQ(press(ui::KEYSYM_CAPSLOCK), hid_report(0, { HID_CAPSLOCK }));
        EXPECT_EQ(release(ui::KEYSYM_CAPSLOCK), hid_report(0));
    }

    void test_modifiers() {
        const u32 syms[8] = {
            ui::KEYSYM_LEFTCTRL, ui::KEYSYM_LEFTSHIFT, ui::KEYSYM_LEFTALT,
            ui::KEYSYM_LEFTMETA, ui::KEYSYM_RIGHTCTRL, ui::KEYSYM_RIGHTSHIFT,
            ui::KEYSYM_RIGHTALT, ui::KEYSYM_RIGHTMETA,
        };

        enumerate(stim, 1);
        for (u32 i = 0; i < 8; i++) {
            EXPECT_EQ(press(syms[i]), hid_report((u8)bit(i)))
                << "modifier " << i;
            EXPECT_EQ(release(syms[i]), hid_report(0)) << "modifier " << i;
        }

        u8 mods = 0;
        for (u32 i = 0; i < 8; i++) {
            mods |= bit(i);
            EXPECT_EQ(press(syms[i]), hid_report(mods)) << "modifier " << i;
        }

        EXPECT_EQ(press(ui::KEYSYM_1), hid_report(0xff, { HID_1 }));
        EXPECT_EQ(release(ui::KEYSYM_1), hid_report(0xff));

        for (u32 i = 0; i < 8; i++) {
            mods &= ~bit(i);
            EXPECT_EQ(release(syms[i]), hid_report(mods)) << "modifier " << i;
        }

        EXPECT_EQ(press(ui::KEYSYM_LEFTCTRL), hid_report(0x01));
        EXPECT_EQ(press(ui::KEYSYM_c), hid_report(0x01, { HID_C }));
        EXPECT_EQ(release(ui::KEYSYM_c), hid_report(0x01));
        EXPECT_EQ(release(ui::KEYSYM_LEFTCTRL), hid_report(0));
    }

    void test_shifted() {
        enumerate(stim, 1);
        input(kbd).notify_key(ui::KEYSYM_A, true);
        EXPECT_EQ(poll(stim), hid_report(0x02));
        EXPECT_EQ(poll(stim), hid_report(0x22));
        EXPECT_EQ(poll(stim), hid_report(0x22, { HID_A }));
        EXPECT_EQ(poll(stim), hid_report(0x20, { HID_A }));
        EXPECT_EQ(poll(stim), hid_report(0x00, { HID_A }));
        EXPECT_EQ(poll(stim), hid_report(0x00, { HID_A }));
        EXPECT_FALSE(input(kbd).has_events());
        EXPECT_EQ(release(ui::KEYSYM_A), hid_report(0));

        EXPECT_EQ(press(ui::KEYSYM_LEFTSHIFT), hid_report(0x02));
        EXPECT_EQ(press(ui::KEYSYM_A), hid_report(0x02, { HID_A }));
        EXPECT_EQ(release(ui::KEYSYM_A), hid_report(0x02));
        EXPECT_EQ(press(ui::KEYSYM_a), hid_report(0x02, { HID_A }));
        EXPECT_EQ(release(ui::KEYSYM_a), hid_report(0x02));
        EXPECT_EQ(release(ui::KEYSYM_LEFTSHIFT), hid_report(0));
    }

    void test_rollover() {
        const u32 syms[] = { ui::KEYSYM_a, ui::KEYSYM_b, ui::KEYSYM_c,
                             ui::KEYSYM_d, ui::KEYSYM_e, ui::KEYSYM_f,
                             ui::KEYSYM_g };
        const vector<u8> six = { HID_A, HID_B, HID_C, HID_D, HID_E, HID_F };

        enumerate(stim, 1);
        EXPECT_EQ(press(ui::KEYSYM_LEFTSHIFT), hid_report(0x02));
        for (size_t i = 0; i < 6; i++)
            press(syms[i]);
        EXPECT_EQ(poll(stim), hid_report(0x02, six));

        vector<u8> rollover(6, HID_ROLLOVER);
        EXPECT_EQ(press(ui::KEYSYM_g), hid_report(0x02, rollover));
        EXPECT_EQ(press(ui::KEYSYM_1), hid_report(0x02, rollover));
        EXPECT_EQ(release(ui::KEYSYM_LEFTSHIFT), hid_report(0x00, rollover));
        EXPECT_EQ(release(ui::KEYSYM_1), hid_report(0x00, rollover));
        EXPECT_EQ(release(ui::KEYSYM_g), hid_report(0x00, six));

        for (u32 sym : syms)
            release(sym);
        EXPECT_EQ(poll(stim), hid_report(0));
    }

    void test_unmapped() {
        ui::keyboard& raw = input(kbd2);
        enumerate(stim2, 2);
        EXPECT_EQ(poll(stim2), hid_report(0));

        raw.notify_key(ui::KEY_A, true);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0, { HID_A }));

        raw.notify_key(ui::KEY_LEFTSHIFT, true);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0x02, { HID_A }));

        raw.notify_key(0, true);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0x02, { HID_A }));
        raw.notify_key(84, true);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0x02, { HID_A }));
        raw.notify_key(200, true);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0x02, { HID_A }));
        raw.notify_key(0x100 | ui::KEY_1, true);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0x02, { HID_A }));

        raw.notify_key(0x100 | ui::KEY_1, false);
        raw.notify_key(200, false);
        raw.notify_key(84, false);
        raw.notify_key(0, false);
        raw.notify_key(ui::KEY_LEFTSHIFT, false);
        raw.notify_key(ui::KEY_A, false);
        EXPECT_EQ(settle(stim2, kbd2), hid_report(0));

        input(kbd).notify_key(0x7fffffff, true);
        EXPECT_FALSE(input(kbd).has_events());
    }

    void test_unconfigured() {
        vector<u8> data;
        stim.usb_out.reset_device();
        stim.addr = 0;
        EXPECT_FALSE(kbd.is_addressed());
        EXPECT_FALSE(kbd.is_configured());
        EXPECT_FAILURE(stim.in(1, data, 8));

        EXPECT_SUCCESS(stim.set_address(7));
        EXPECT_TRUE(kbd.is_addressed());
        EXPECT_EQ(kbd.get_address(), 7);
        EXPECT_FAILURE(stim.in(1, data, 8));

        EXPECT_FAILURE(stim.set_configuration(1));
        EXPECT_FALSE(kbd.is_configured());
        EXPECT_SUCCESS(stim.set_configuration(42));
        EXPECT_TRUE(kbd.is_configured());
        EXPECT_SUCCESS(
            stim.control_in(USB_REQ_GET_CONFIGURATION, 0, 0, data, 1));
        EXPECT_EQ(data, vector<u8>({ 42 }));
        EXPECT_SUCCESS(stim.control_in(USB_REQ_GET_STATUS, 0, 0, data, 2));
        EXPECT_EQ(data, vector<u8>({ 0x02, 0x00 }));
        EXPECT_EQ(poll(stim), hid_report(0));

        stim.addr = 3;
        EXPECT_EQ(stim.in(1, data, 8), USB_RESULT_NODEV);
        stim.addr = 7;

        EXPECT_SUCCESS(stim.set_configuration(0));
        EXPECT_FALSE(kbd.is_configured());
        EXPECT_FAILURE(stim.in(1, data, 8));

        EXPECT_SUCCESS(stim.set_configuration(42));
        input(kbd).notify_key(ui::KEYSYM_a, true);
        EXPECT_EQ(settle(stim, kbd), hid_report(0, { HID_A }));

        stim.usb_out.reset_device();
        EXPECT_FALSE(kbd.is_configured());
        EXPECT_FAILURE(stim.in(1, data, 8));

        enumerate(stim, 1);
        EXPECT_EQ(poll(stim), hid_report(0, { HID_A }));
        EXPECT_EQ(release(ui::KEYSYM_a), hid_report(0));
    }

    void test_endpoints() {
        vector<u8> data;
        enumerate(stim, 1);
        EXPECT_FAILURE(stim.in(2, data, 8));
        EXPECT_FAILURE(stim.in(1, data, 4));
        EXPECT_FAILURE(stim.in(1, data, 16));
        EXPECT_FAILURE(stim.out(1, vector<u8>(8, 0)));
        EXPECT_SUCCESS(stim.out(1, nullptr, 0));
        EXPECT_EQ(poll(stim), hid_report(0));
    }
};

TEST(usb, keyboard) {
    broker broker("test");
    broker.define("test.kbd2.usb3", false);
    broker.define("test.kbd2.vendorid", 0xabcd);
    broker.define("test.kbd2.productid", 0x1234);
    broker.define("test.kbd2.manufacturer", "Test Vendor");
    broker.define("test.kbd2.product", "Test Keyboard");
    broker.define("test.kbd2.serialno", "12345");
    broker.define("test.kbd2.keymap", "");

    usb_keyboard_test test("test");
    sc_core::sc_start();
}
