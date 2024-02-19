/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_DEVICE_H
#define VCML_USB_DEVICE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/usb.h"

namespace vcml {
namespace usb {

class device : public module, public usb_dev_if
{
private:
    u32 m_address;
    bool m_stalled;

    enum device_state {
        STATE_DEFAULT = 0,
        STATE_ADDRESSED = 1,
        STATE_CONFIGURED = 2,
    };

    device_state m_state;

    enum control_state {
        STATE_SETUP,
        STATE_DATA,
        STATE_STATUS,
    };

    struct ep0 {
        u16 req;
        u16 val;
        u16 idx;
        u16 len;
        size_t pos;
        usb_result res;
        control_state state;
        u8 buf[1024];
    } m_ep0;

public:
    bool is_addressed() const { return m_state >= STATE_ADDRESSED; }
    bool is_configured() const { return m_state >= STATE_CONFIGURED; }

    u32 get_address() const { return m_address; }

    device(const sc_module_name& nm);
    virtual ~device();
    VCML_KIND(usb::device);

protected:
    virtual usb_result get_configuration(u8& config);
    virtual usb_result set_configuration(u8 config);

    virtual usb_result get_desc(usb_device_desc& desc) = 0;
    virtual usb_result get_desc(usb_config_desc& desc, size_t idx) = 0;
    virtual usb_result get_desc(usb_string_desc& str, size_t idx) = 0;
    virtual usb_result get_desc(usb_bos_desc& desc) = 0;

    virtual usb_result handle_control(u16 req, u16 val, u16 idx, u8* data,
                                      size_t length);
    virtual usb_result handle_ep0(usb_packet& p);

    virtual void usb_reset_device() override;
    virtual void usb_reset_endpoint(int ep) override;
    virtual void usb_transport(usb_packet& p) override;
};

} // namespace usb
} // namespace vcml

#endif
