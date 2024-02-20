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

enum keyboard_strids : u8 {
    STRID_MANUFACTURER = 1,
    STRID_PRODUCT,
    STRID_SERIALNO,
    STRID_CONFIGURATION,
};

keyboard::keyboard(const sc_module_name& nm):
    device(nm),
    m_keyboard(name()),
    m_console(),
    usb3("usb3", true),
    vendorid("vendorid", 0xabcd),
    productid("productid", 0x1),
    manufacturer("manufacturer", "MachineWare GmbH"),
    product("product", "VCML Keyboard"),
    serialno("serialno", VCML_GIT_REV_SHORT),
    usb_in("usb_in") {
    // TODO: connect display
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

usb_result keyboard::get_desc(usb_device_desc& desc) {
    if (usb3) {
        desc.bcd_usb = 0x300;
        desc.max_packet_size0 = 9;
    } else {
        desc.bcd_usb = 0x200;
        desc.max_packet_size0 = 8;
    }

    desc.vendor_id = vendorid;
    desc.product_id = productid;
    desc.bcd_device = 1;
    desc.manufacturer_strid = STRID_MANUFACTURER;
    desc.product_strid = STRID_PRODUCT;
    desc.serial_number_strid = 3;
    desc.num_configurations = 1;
    return USB_RESULT_SUCCESS;
}

usb_result keyboard::get_desc(usb_config_desc& desc, size_t idx) {
    if (idx > 0) {
        log_error("invalid configuration descriptor index: %zu", idx);
        return USB_RESULT_STALL;
    }

    desc.total_length = sizeof(usb_config_desc) + sizeof(usb_interface_desc) +
                        sizeof(usb_endpoint_desc);
    desc.num_interfaces = 1;
    desc.configuration_value = 42;
    desc.configuration_strid = STRID_CONFIGURATION;
    desc.attributes = USB_CFG_ONE | USB_CFG_WAKEUP;
    desc.max_power = 40; // 80mA

    return USB_RESULT_SUCCESS;
}

usb_result keyboard::get_desc(usb_interface_desc& desc, size_t idx,
                              size_t cfg) {
    if (idx != 0) {
        log_error("invalid interface id: %zu", idx);
        return USB_RESULT_STALL;
    }

    if (cfg != 0) {
        log_error("invalid configuration id: %zu", cfg);
        return USB_RESULT_STALL;
    }

    desc.num_endpoints = 1;
    desc.interface_class = USB_CLASS_HID;
    desc.interface_subclass = 1; // boot
    desc.interface_protocol = 1; // keyboard protocol

    return USB_RESULT_SUCCESS;
}

usb_result keyboard::get_desc(usb_endpoint_desc& desc, size_t idx, size_t ifx,
                              size_t cfg) {
    if (idx != 0) {
        log_error("invalid interface id: %zu", idx);
        return USB_RESULT_STALL;
    }

    if (ifx != 0) {
        log_error("invalid interface id: %zu", ifx);
        return USB_RESULT_STALL;
    }

    if (cfg != 0) {
        log_error("invalid configuration id: %zu", cfg);
        return USB_RESULT_STALL;
    }

    desc.endpoint_address = 0x81;
    desc.attributes = USB_EP_IRQ;
    desc.max_packet_size = 8;
    desc.interval = 10; // 125us units

    return USB_RESULT_SUCCESS;
}

static void setup_string_desc(usb_string_desc& desc, const string& s) {
    desc.length = sizeof(desc) + 2 * s.length();
    for (size_t i = 0; i < s.length(); i++)
        desc.wstring[i] = s[i];
}

usb_result keyboard::get_desc(usb_string_desc& desc, size_t idx) {
    switch (idx) {
    case 0:
        desc.length = sizeof(desc) + 2;
        desc.wstring[0] = 0x0409;
        return USB_RESULT_SUCCESS;

    case STRID_MANUFACTURER:
        setup_string_desc(desc, manufacturer);
        return USB_RESULT_SUCCESS;

    case STRID_PRODUCT:
        setup_string_desc(desc, product);
        return USB_RESULT_SUCCESS;

    case STRID_SERIALNO:
        setup_string_desc(desc, serialno);
        return USB_RESULT_SUCCESS;

    case STRID_CONFIGURATION:
        setup_string_desc(desc, "HID Keyboard");
        return USB_RESULT_SUCCESS;

    default:
        return USB_RESULT_STALL;
    }
}

usb_result keyboard::get_desc(usb_bos_desc& desc) {
    desc.total_length = sizeof(desc);
    desc.num_device_caps = 0;
    return USB_RESULT_SUCCESS;
}

VCML_EXPORT_MODEL(vcml::usb::keyboard, name, args) {
    return new keyboard(name);
}

} // namespace usb
} // namespace vcml
