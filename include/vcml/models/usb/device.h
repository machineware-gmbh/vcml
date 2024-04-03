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

struct endpoint_desc {
    u8 address;
    u8 attributes;
    u16 max_packet_size;
    u8 interval;
    u8 refresh;
    u8 sync_address;
    vector<u8> extra;
};

struct interface_desc {
    u8 alternate_setting;
    u8 ifxclass;
    u8 subclass;
    u8 protocol;
    vector<endpoint_desc> endpoints;
    vector<u8> extra;
};

struct config_desc {
    u8 value;
    u8 attributes;
    u8 max_power;
    vector<interface_desc> interfaces;
};

struct device_desc {
    u16 bcd_usb;
    u8 device_class;
    u8 device_subclass;
    u8 device_protocol;
    u8 max_packet_size0;
    u16 vendor_id;
    u16 product_id;
    u16 bcd_device;
    string manufacturer;
    string product;
    string serial_number;
    vector<config_desc> configs;
};

constexpr usb_speed usb_speed_max(const device_desc& desc) {
    switch (desc.bcd_usb) {
    case 0x100:
        return USB_SPEED_LOW;
    case 0x110:
        return USB_SPEED_FULL;
    case 0x200:
        return USB_SPEED_HIGH;
    case 0x300:
        return USB_SPEED_SUPER;
    default:
        return USB_SPEED_NONE;
    }
}

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
        u8* ptr;
        u8 buf[1u << 16];
    } m_ep0;

    bool cmd_usb_attach(const vector<string>& args, ostream& os);
    bool cmd_usb_detach(const vector<string>& args, ostream& os);

public:
    property<bool> start_attached;

    bool is_addressed() const { return m_state >= STATE_ADDRESSED; }
    bool is_configured() const { return m_state >= STATE_CONFIGURED; }

    u32 get_address() const { return m_address; }

    device(const sc_module_name& nm, const device_desc& desc);
    virtual ~device();
    VCML_KIND(usb::device);

protected:
    device_desc m_desc;
    size_t m_cur_config;
    size_t m_cur_iface;

    virtual void start_of_simulation() override;

    vector<usb_target_socket*> all_usb_sockets();
    usb_target_socket* find_usb_socket(const string& name);
    usb_target_socket* find_usb_socket(const string& name, size_t idx);

    virtual usb_result get_data(u32 ep, u8* data, size_t len);
    virtual usb_result set_data(u32 ep, const u8* data, size_t len);

    virtual usb_result get_configuration(u8& config);
    virtual usb_result set_configuration(u8 config);
    virtual usb_result get_interface(u8& interface);
    virtual usb_result get_descriptor(u8 type, u8 idx, u8* data, size_t size);

    virtual usb_result handle_control(u16 req, u16 val, u16 idx, u8* data,
                                      size_t length);
    virtual usb_result handle_ep0(usb_packet& p);
    virtual usb_result handle_data(usb_packet& p);

    virtual void usb_reset_device() override;
    virtual void usb_reset_endpoint(int ep) override;
    virtual void usb_transport(usb_packet& p) override;
};

} // namespace usb
} // namespace vcml

#endif
