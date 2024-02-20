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
    ui::keyboard m_keyboard;
    ui::console m_console;

public:
    property<bool> usb3;

    property<u16> vendorid;
    property<u16> productid;

    property<string> manufacturer;
    property<string> product;
    property<string> serialno;

    usb_target_socket usb_in;

    keyboard(const sc_module_name& nm);
    virtual ~keyboard();
    VCML_KIND(usb::keyboard);

protected:
    virtual void start_of_simulation() override;
    virtual void end_of_simulation() override;

    virtual usb_result get_desc(usb_device_desc& desc) override;
    virtual usb_result get_desc(usb_config_desc& desc, size_t idx) override;
    virtual usb_result get_desc(usb_interface_desc& desc, size_t idx,
                                size_t cfg) override;
    virtual usb_result get_desc(usb_endpoint_desc& desc, size_t idx,
                                size_t ifx, size_t cfg) override;
    virtual usb_result get_desc(usb_string_desc& desc, size_t idx) override;
    virtual usb_result get_desc(usb_bos_desc& desc) override;
};

} // namespace usb
} // namespace vcml

#endif
