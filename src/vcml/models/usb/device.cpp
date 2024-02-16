/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/device.h"

namespace vcml {
namespace usb {

device::device(const sc_module_name& nm):
    module(nm), usb_dev_if(), m_address(0), m_stalled(false), m_ep0() {
    // nothing to do
}

device::~device() {
    // nothing to do
}

usb_result device::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                  size_t length) {
    switch (req) {
    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ADDRESS:
        log_info("received set_address(%hu) request", val);
        m_address = val;
        m_state = STATE_ADDRESSED;
        return USB_RESULT_SUCCESS;

    case USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_DESCRIPTOR:
        log_info("host wants device descriptor... TODO");
        sc_stop();
        return USB_RESULT_NODEV;

    default:
        log_info("unknown request 0x%04hx val:0x%04hx idx:0x%04hx %zu bytes",
                 req, val, idx, length);
        return USB_RESULT_STALL;
    }
}

usb_result device::handle_ep0(usb_packet& p) {
    switch (p.token) {
    case USB_TOKEN_SETUP: {
        if (p.length != 8) {
            m_stalled = true;
            return USB_RESULT_STALL;
        }

        m_stalled = false;
        m_ep0.req = p.data[0] | (u16)p.data[1] << 8;
        m_ep0.val = p.data[2] | (u16)p.data[3] << 8;
        m_ep0.idx = p.data[4] | (u16)p.data[5] << 8;
        m_ep0.len = p.data[6] | (u16)p.data[7] << 8;

        if (m_ep0.len > sizeof(m_ep0.buf))
            return USB_RESULT_BABBLE;

        usb_result res = USB_RESULT_SUCCESS;
        if (m_ep0.req & USB_REQ_IN) {
            res = handle_control(m_ep0.req, m_ep0.val, m_ep0.idx, m_ep0.buf,
                                 m_ep0.len);
        }

        if (failed(res))
            return res;

        m_ep0.state = m_ep0.len ? STATE_DATA : STATE_STATUS;
    }

    case USB_TOKEN_IN: {
        switch (m_ep0.state) {
        case STATE_STATUS: {
            usb_result res = USB_RESULT_SUCCESS;
            if (!(m_ep0.req & USB_REQ_IN)) {
                res = handle_control(m_ep0.req, m_ep0.val, m_ep0.idx,
                                     m_ep0.buf, m_ep0.len);
            }

            m_ep0.state = STATE_SETUP;
            return res;
        }

        default:
            return USB_RESULT_SUCCESS;
        }
    }

    case USB_TOKEN_OUT: {
        log_info("USB_TOKEN_OUT");
        return USB_RESULT_SUCCESS;
    }

    default:
        return USB_RESULT_NODEV;
    }
}

void device::usb_reset_device() {
    log_info("usb reset device");
    usb_reset_endpoint(0);
}

void device::usb_reset_endpoint(int ep) {
    log_info("usb reset endpoint %d", ep);
    switch (ep) {
    case 0:
        m_ep0.req = 0;
        m_ep0.state = STATE_SETUP;
    }
}

void device::usb_transport(usb_packet& p) {
    if (is_addressed() && p.addr != m_address) {
        p.result = USB_RESULT_NODEV;
        return;
    }

    if (p.epno == 0) {
        p.result = handle_ep0(p);
        m_stalled |= p.result == USB_RESULT_STALL;
        return;
    }

    if (m_stalled) {
        p.result = USB_RESULT_STALL;
        return;
    }

    log_info("%s", to_string(p).c_str());
}

} // namespace usb
} // namespace vcml
