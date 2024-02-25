/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_KEYBOARD_H
#define VCML_USB_KEYBOARD_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/ui/input.h"
#include "vcml/ui/keymap.h"
#include "vcml/ui/console.h"

#include "vcml/protocols/usb.h"
#include "vcml/models/usb/device.h"

namespace vcml {
namespace usb {

class keyboard : public device
{
private:
    u8 m_leds;
    vector<u8> m_keys;

    ui::keyboard m_keyboard;
    ui::console m_console;

    void poll_modifier_keys(u8& data);
    void poll_standard_keys(u8* data, size_t len);
    void poll_keys(u8* data, size_t len);

public:
    enum led_type {
        LED_NUM_LOCK = bit(0),
        LED_CAPS_LOCK = bit(1),
        LED_SCROLL_LOCK = bit(2),
        LED_COMPOSE = bit(3),
        LED_KANA = bit(4),
    };

    constexpr bool get_led(led_type type) const { return m_leds & type; }

    property<bool> usb3;

    property<u16> vendorid;
    property<u16> productid;

    property<string> manufacturer;
    property<string> product;
    property<string> serialno;
    property<string> keymap;

    usb_target_socket usb_in;

    keyboard(const sc_module_name& nm);
    virtual ~keyboard();
    VCML_KIND(usb::keyboard);

protected:
    virtual void start_of_simulation() override;
    virtual void end_of_simulation() override;

    virtual usb_result get_report(u8* data, size_t size);
    virtual usb_result set_report(u8* data, size_t size);

    virtual usb_result get_data(u32 ep, u8* data, size_t len) override;

    usb_result get_interface_descriptor(u8 type, u8 idx, u8* data, size_t sz);

    virtual usb_result handle_control(u16 req, u16 val, u16 idx, u8* data,
                                      size_t length) override;
};

} // namespace usb
} // namespace vcml

#endif
