/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/keyboard.h"
#include "vcml/core/version.h"

namespace vcml {
namespace usb {

static const device_desc KEYBOARD_DESC{
    0x200,              // bcd_usb
    0,                  // device class
    0,                  // device subclass
    0,                  // device protocol
    8,                  // max packet size
    0xaaaa,             // vendor id
    0x1234,             // device id
    0x0,                // bcd_device
    "MachineWare GmbH", // vendor string
    "VCML Keyboard",    // product string
    VCML_GIT_REV_SHORT, // serial number string
    {
        {
            // configurations
            42,                           // configuration0 value
            USB_CFG_ONE | USB_CFG_WAKEUP, // config0 attributes
            40,                           // config0 power (80mA)
            {
                // interfaces
                {
                    0,             // interface0 alternate setting
                    USB_CLASS_HID, // interface0 class
                    1,             // interface0 subclass (boot)
                    1,             // interface0 protocol (keyboard)
                    {
                        // endpoints
                        {
                            usb_ep_in(1), // endpoint1 address
                            USB_EP_IRQ,   // endpoint1 attributes
                            8,            // endpoint1 max packet size
                            8,            // endpoint1 intervall (125us units)
                            0,            // endpoint1 refresh
                            0,            // endpoint1 sync address
                        },
                    },
                    {
                        // extra descriptors
                        {
                            0x09,
                            USB_DT_HID,
                            0x11,
                            0x01,
                            0x00,
                            0x01,
                            USB_DT_REPORT,
                            0x3f,
                            0x00,
                        },
                    },
                },
            },
        },
    },
};

static const u8 USB3_ENDPOINT_COMPANION_DT[] = {
    0x06, // length
    USB_DT_ENDPOINT_COMPANION,
    0x00, // max burst
    0x00, // attributes
    0x08, // bytes per interval (lo)
    0x00  // bytes per interval (hi)
};

// USB-HID: Appendix B (Boot Interface Descriptors)
// - input reports (i.e. reports from device to host)
//   - 1x8 bits for modifier keys
//   - 1x8 bits reserved
//   - 1x5 bits for LED state
//   - 1x3 bits padding
//   - 6x8 bits for keys
// - output reports (i.e. reports from host to device)
//   - 1x5 bits for LED state
//   - 1x3 bits padding
static const u8 KEYBOARD_REPORT_DESCRIPTOR[] = {
    0x05, 0x01, // usage page (generic desktop)
    0x09, 0x06, // usage (keyboard)
    0xa1, 0x01, // collection (application)
    0x75, 0x01, //   report size (1)
    0x95, 0x08, //   report count (8)
    0x05, 0x07, //   usage page (key codes)
    0x19, 0xe0, //   usage minimum (224)
    0x29, 0xe7, //   usage maximum (231)
    0x15, 0x00, //   logical minimum (0)
    0x25, 0x01, //   logical maximum (1)
    0x81, 0x02, //   input (data, variable, absolute)
    0x95, 0x01, //   report count (1)
    0x75, 0x08, //   report size (8)
    0x81, 0x01, //   input (constant)
    0x95, 0x05, //   report count (5)
    0x75, 0x01, //   report size (1)
    0x05, 0x08, //   usage page (LEDs)
    0x19, 0x01, //   usage minimum (1)
    0x29, 0x05, //   usage maximum (5)
    0x91, 0x02, //   output (data, variable, absolute) [LED state]
    0x95, 0x01, //   report count (1)
    0x75, 0x03, //   report size (3)
    0x91, 0x01, //   output (constant)
    0x95, 0x06, //   report count (6)
    0x75, 0x08, //   report size (8)
    0x15, 0x00, //   logical minimum (0)
    0x25, 0xff, //   logical maximum (255)
    0x05, 0x07, //   usage page (key codes)
    0x19, 0x00, //   usage minimum (0)
    0x29, 0xff, //   usage maximum (255)
    0x81, 0x00, //   input (data, array)
    0xc0,       // end collection
};

static const u8 USB_HID_KEYCODE_TABLE[256] = {
    0x00, 0x29, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, // [0..7]
    0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, // [8..15]
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, // [16..23]
    0x12, 0x13, 0x2f, 0x30, 0x28, 0xe0, 0x04, 0x16, // [24..31]
    0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, // [32..39]
    0x34, 0x35, 0xe1, 0x31, 0x1d, 0x1b, 0x06, 0x19, // [40..47]
    0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xe5, 0x55, // [48..55]
    0xe2, 0x2c, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, // [56..63]
    0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5f, // [64..71]
    0x60, 0x61, 0x56, 0x5c, 0x5d, 0x5e, 0x57, 0x59, // [72..79]
    0x5a, 0x5b, 0x62, 0x63, 0xff, 0x94, 0x64, 0x44, // [80..87]
    0x45, 0x87, 0x92, 0x93, 0x8a, 0x88, 0x8b, 0x8c, // [88..95]
    0x58, 0xe4, 0x54, 0x46, 0xe6, 0x00, 0x4a, 0x52, // [96..103]
    0x4b, 0x50, 0x4f, 0x4d, 0x51, 0x4e, 0x49, 0x4c, // [104..111]
    0x00, 0x7f, 0x81, 0x80, 0x66, 0x67, 0x00, 0x48, // [112..119]
    0x00, 0x85, 0x90, 0x91, 0x89, 0xe3, 0xe7, 0x65, // [120..127]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [128..135]
    0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, // [136..143]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [144..151]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [152..159]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [160..167]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [168..175]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [176..183]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [184..191]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [192..199]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [200..207]
    0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, // [208..215]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [216..223]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [224..231]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [232..239]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [240..247]
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // [248..255]
};

enum usb_hid_request : u16 {
    USB_REQ_GET_REPORT = 0x01,
    USB_REQ_GET_IDLE = 0x02,
    USB_REQ_GET_PROTOCOL = 0x03,
    USB_REQ_SET_REPORT = 0x09,
    USB_REQ_SET_IDLE = 0x0a,
    USB_REQ_SET_PROTOCOL = 0x0b,
};

void keyboard::poll_modifier_keys(u8& data) {
    data = 0;
    if (m_keyboard.ctrl_l())
        data |= bit(0);
    if (m_keyboard.shift_l())
        data |= bit(1);
    if (m_keyboard.alt_l())
        data |= bit(2);
    if (m_keyboard.meta_l())
        data |= bit(3);
    if (m_keyboard.ctrl_r())
        data |= bit(4);
    if (m_keyboard.shift_r())
        data |= bit(5);
    if (m_keyboard.alt_r())
        data |= bit(6);
    if (m_keyboard.meta_r())
        data |= bit(7);
}

void keyboard::poll_standard_keys(u8* data, size_t length) {
    ui::input_event ev;
    if (m_keyboard.pop_event(ev) && ev.is_key()) {
        u8 hid = USB_HID_KEYCODE_TABLE[ev.key.code & 0xff];
        if (ev.key.state == ui::VCML_KEY_UP)
            stl_remove(m_keys, hid);
        else
            stl_add_unique(m_keys, hid);
    }

    if (m_keys.size() <= length) {
        memcpy(data, m_keys.data(), m_keys.size());
        memset(data + m_keys.size(), 0, length - m_keys.size());
    } else {
        memcpy(data, m_keys.data(), length - 1);
        data[length - 1] = 1; // rollover
    }
}

void keyboard::poll_keys(u8* data, size_t length) {
    VCML_ERROR_ON(length < 8, "insufficient buffer size to poll keys");
    poll_modifier_keys(data[0]);
    data[1] = 0; // reserved
    poll_standard_keys(data + 2, length - 2);
}

keyboard::keyboard(const sc_module_name& nm):
    device(nm, KEYBOARD_DESC),
    m_leds(),
    m_keyboard(name()),
    m_console(),
    usb3("usb3", true),
    vendorid("vendorid", 0xabcd),
    productid("productid", 0x1),
    manufacturer("manufacturer", "MachineWare GmbH"),
    product("product", "VCML Keyboard"),
    serialno("serialno", VCML_GIT_REV_SHORT),
    keymap("keymap", "us"),
    usb_in("usb_in") {
    m_keyboard.set_layout(keymap);
    m_desc.vendor_id = vendorid;
    m_desc.product_id = productid;
    m_desc.manufacturer = manufacturer;
    m_desc.product = product;
    m_desc.serial_number = serialno;

    if (usb3) {
        m_desc.bcd_usb = 0x300;
        m_desc.max_packet_size0 = 9;
        auto& extra = m_desc.configs[0].interfaces[0].endpoints[0].extra;
        for (auto ch : USB3_ENDPOINT_COMPANION_DT)
            extra.push_back(ch);
    }
}

keyboard::~keyboard() {
    // nothing to do
}

void keyboard::start_of_simulation() {
    device::start_of_simulation();
    if (m_console.has_display())
        m_console.notify(m_keyboard);
}

void keyboard::end_of_simulation() {
    m_console.shutdown();
    device::end_of_simulation();
}

usb_result keyboard::get_report(u8* data, size_t length) {
    if (length != 8) {
        log_error("invalid keyboard input report length: %zu", length);
        return USB_RESULT_STALL;
    }

    poll_keys(data, length);
    return USB_RESULT_SUCCESS;
}

usb_result keyboard::set_report(u8* data, size_t length) {
    if (length != 1) {
        log_error("invalid keyboard output report length: %zu", length);
        return USB_RESULT_STALL;
    }

    log_debug("set_report(0x%02hhx)", *data);

    if ((m_leds ^ *data) & LED_NUM_LOCK)
        log_info("num-lock led %s", *data & LED_NUM_LOCK ? "on" : "off");
    if ((m_leds ^ *data) & LED_CAPS_LOCK)
        log_info("caps-lock led %s", *data & LED_CAPS_LOCK ? "on" : "off");
    if ((m_leds ^ *data) & LED_SCROLL_LOCK)
        log_info("scroll-lock led %s", *data & LED_SCROLL_LOCK ? "on" : "off");
    if ((m_leds ^ *data) & LED_COMPOSE)
        log_info("compose led %s", *data & LED_COMPOSE ? "on" : "off");
    if ((m_leds ^ *data) & LED_KANA)
        log_info("kana led %s", *data & LED_KANA ? "on" : "off");

    m_leds = *data;
    return USB_RESULT_SUCCESS;
}

usb_result keyboard::get_data(u32 ep, u8* data, size_t len) {
    if (ep != 1) {
        log_error("invalid endpoint contacted: %u", ep);
        return USB_RESULT_NACK;
    }

    if (len != 8) {
        log_error("invalid data packet length: %zu, expected 8", len);
        return USB_RESULT_STALL;
    }

    poll_keys(data, len);
    return USB_RESULT_SUCCESS;
}

usb_result keyboard::get_interface_descriptor(u8 type, u8 index, u8* data,
                                              size_t size) {
    log_debug("get_interface_descriptor(%hhu, %hhu)", type, index);
    switch (type) {
    case USB_DT_REPORT:
        memcpy(data, KEYBOARD_REPORT_DESCRIPTOR,
               min(size, sizeof(KEYBOARD_REPORT_DESCRIPTOR)));
        return USB_RESULT_SUCCESS;
    default:
        log_error("unknown interface descriptor: 0x%02hhx", type);
        return USB_RESULT_STALL;
    }
}

usb_result keyboard::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                    size_t length) {
    switch (req) {
    case USB_REQ_IN | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_GET_REPORT: {
        log_debug("get_report(%zu)", length);
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_IN | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_GET_IDLE: {
        log_debug("get_idle(%zu)", length);
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_IN | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_GET_PROTOCOL: {
        log_debug("get_protocol(%zu)", length);
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_OUT | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_SET_REPORT: {
        return set_report(data, length);
    }

    case USB_REQ_OUT | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_SET_IDLE: {
        log_debug("set_idle(%hu)", val);
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_OUT | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_SET_PROTOCOL: {
        log_debug("set_protocol(%hu)", val);
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_IN | USB_REQ_IFACE | USB_REQ_GET_DESCRIPTOR: {
        u8 type = val >> 8;
        u8 index = val & 0xff;
        return get_interface_descriptor(type, index, data, length);
    }

    default:
        return device::handle_control(req, val, idx, data, length);
    }
}

VCML_EXPORT_MODEL(vcml::usb::keyboard, name, args) {
    return new keyboard(name);
}

} // namespace usb
} // namespace vcml
