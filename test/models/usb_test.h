/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TEST_MODELS_USB_TEST_H
#define VCML_TEST_MODELS_USB_TEST_H

#include "testing.h"

class usb_stim : public module, public usb_host_if
{
public:
    usb_initiator_socket usb_out;

    u32 addr;
    size_t packet_size;
    size_t attached;
    size_t detached;

    usb_stim(const sc_module_name& nm):
        module(nm),
        usb_host_if(),
        usb_out("usb_out"),
        addr(0),
        packet_size(64),
        attached(0),
        detached(0) {}

    virtual ~usb_stim() = default;

    virtual void usb_attach(usb_initiator_socket& socket) override {
        attached++;
    }

    virtual void usb_detach(usb_initiator_socket& socket) override {
        detached++;
    }

    usb_result setup(u16 req, u16 val, u16 idx, u16 len) {
        u8 data[8] = {
            (u8)(req >> 8), (u8)req,        (u8)val, (u8)(val >> 8),
            (u8)idx,        (u8)(idx >> 8), (u8)len, (u8)(len >> 8),
        };

        usb_packet p = usb_packet_setup(addr, data, sizeof(data));
        usb_out.send(p);
        return p.result;
    }

    usb_result in(u32 ep, void* data, size_t len) {
        usb_packet p = usb_packet_in(addr, ep, data, len);
        usb_out.send(p);
        return p.result;
    }

    usb_result out(u32 ep, const void* data, size_t len) {
        usb_packet p = usb_packet_out(addr, ep, data, len);
        usb_out.send(p);
        return p.result;
    }

    usb_result in(u32 ep, vector<u8>& data, size_t len) {
        data.assign(len, 0);
        return in(ep, data.data(), len);
    }

    usb_result out(u32 ep, const vector<u8>& data) {
        return out(ep, data.data(), data.size());
    }

    usb_result control_in(u16 req, u16 val, u16 idx, vector<u8>& data,
                          size_t len) {
        data.assign(len, 0);
        usb_result res = setup(req | USB_REQ_IN, val, idx, (u16)len);
        if (failed(res))
            return res;

        for (size_t pos = 0; pos < len; pos += packet_size) {
            res = in(0, data.data() + pos, min(packet_size, len - pos));
            if (failed(res))
                return res;
        }

        return out(0, nullptr, 0);
    }

    usb_result control_out(u16 req, u16 val, u16 idx,
                           const vector<u8>& data = {}) {
        size_t len = data.size();
        usb_result res = setup(req & ~USB_REQ_IN, val, idx, (u16)len);
        if (failed(res))
            return res;

        for (size_t pos = 0; pos < len; pos += packet_size) {
            res = out(0, data.data() + pos, min(packet_size, len - pos));
            if (failed(res))
                return res;
        }

        return in(0, nullptr, 0);
    }

    usb_result get_descriptor(u8 type, u8 idx, vector<u8>& data, size_t len,
                              u16 req = USB_REQ_DEVICE) {
        u16 val = (u16)type << 8 | idx;
        return control_in(req | USB_REQ_GET_DESCRIPTOR, val, 0, data, len);
    }

    usb_result set_address(u32 address) {
        usb_result res = control_out(USB_REQ_DEVICE | USB_REQ_SET_ADDRESS,
                                     (u16)address, 0);
        if (success(res))
            addr = address;
        return res;
    }

    usb_result set_configuration(u8 config) {
        return control_out(USB_REQ_DEVICE | USB_REQ_SET_CONFIGURATION, config,
                           0);
    }

    usb_result set_interface(u16 ifx, u16 altset) {
        return control_out(USB_REQ_IFACE | USB_REQ_SET_INTERFACE, altset, ifx);
    }

    string get_string(u8 idx) {
        vector<u8> data;
        if (failed(get_descriptor(USB_DT_STRING, idx, data, 255)))
            return "";

        string str;
        for (size_t i = 2; i + 1 < data[0] && i + 1 < data.size(); i += 2)
            str += (char)data[i];
        return str;
    }
};

#endif
