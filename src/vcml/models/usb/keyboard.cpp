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
    0,                  // bcd_usb
    0,                  // device class
    0,                  // device subclass
    0,                  // device protocol
    0,                  // max packet size
    0xaaaa,             // vendor id
    0x1234,             // device id
    0x0,                // bcd_device
    "MachineWare GmbH", // vendor string
    "VCML Keyboard",    // product string
    VCML_GIT_REV_SHORT, // serial number string
    { {
        // configurations
        42,                           // configuration0 value
        USB_CFG_ONE | USB_CFG_WAKEUP, // config0 attributes
        40,                           // config0 power (80mA)
        { {
            0,             // interface0 alternate setting
            USB_CLASS_HID, // interface0 class
            1,             // interface0 subclass (boot)
            1,             // interface0 protocol (keyboard)
            { {
                0x81,       // endpoint address
                USB_EP_IRQ, // endpoint attributes
                8,          // endpoint max packet size
                10,         // endpoint intervall (125us units)
                0,          // endpoint refresh
                0,          // endpoint sync address
            } },
            { {
                0x09,
                USB_DT_HID,
                0x11,
                0x01,
                0x00,
                0x01,
                USB_DT_REPORT,
                0x3f,
                0x00,
            } },
        } },
    } },
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

enum usb_hid_request : u16 {
    USB_REQ_GET_REPORT = 0x01,
    USB_REQ_GET_IDLE = 0x02,
    USB_REQ_GET_PROTOCOL = 0x03,
    USB_REQ_SET_REPORT = 0x09,
    USB_REQ_SET_IDLE = 0x0a,
    USB_REQ_SET_PROTOCOL = 0x0b,
};

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
    usb_in("usb_in") {
    if (usb3) {
        m_desc.bcd_usb = 0x300;
        m_desc.max_packet_size0 = 9;
    } else {
        m_desc.bcd_usb = 0x200;
        m_desc.max_packet_size0 = 8;
    }
}

keyboard::~keyboard() {
    // nothing to do
}

void keyboard::start_of_simulation() {
    if (usb3)
        usb_in.attach(USB_SPEED_SUPER);
    else
        usb_in.attach(USB_SPEED_HIGH);
}

void keyboard::end_of_simulation() {
    m_console.shutdown();
    module::end_of_simulation();
}

usb_result keyboard::get_report(u8* data, size_t length) {
    if (length != 8) {
        log_error("invalid keyboard input report length: %zu", length);
        return USB_RESULT_STALL;
    }

    data[0] = 0; // TODO: modifier state
    data[1] = 0; // TODO: reserved
    data[2] = 0; // TODO: keycode 1
    data[3] = 0; // TODO: keycode 2
    data[4] = 0; // TODO: keycode 3
    data[5] = 0; // TODO: keycode 4
    data[6] = 0; // TODO: keycode 5
    data[7] = 0; // TODO: keycode 6

    log_debug(
        "get_report(%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx)",
        data[0], data[1], data[2], data[3], data[4], data[5], data[6],
        data[7]);

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
    log_info("ep%u get_data (%zu bytes)", ep, len);
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
