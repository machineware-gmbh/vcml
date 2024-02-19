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

usb_result device::get_configuration(u8& config) {
    config = 0;
    return USB_RESULT_SUCCESS;
}

usb_result device::set_configuration(u8 config) {
    return USB_RESULT_SUCCESS;
}

usb_result device::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                  size_t length) {
    switch (req) {
    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ADDRESS: {
        log_debug("set_address(%hu)", val);
        m_address = val;
        m_state = STATE_ADDRESSED;
        return USB_RESULT_SUCCESS;
    }

    case USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_DESCRIPTOR: {
        memset(data, 0, length);
        u32 type = val >> 8;
        u32 index = val & 0xff;

        log_debug("get_descriptor(%s, %u)", usb_desc_str(type), index);

        switch (type) {
        case USB_DT_DEVICE:
            data[0] = sizeof(usb_device_desc);
            data[1] = USB_DT_DEVICE;
            return get_desc(*(usb_device_desc*)data);

        case USB_DT_CONFIG:
            data[0] = sizeof(usb_config_desc);
            data[1] = USB_DT_CONFIG;
            return get_desc(*(usb_config_desc*)data, index);

        case USB_DT_STRING:
            data[0] = sizeof(usb_string_desc);
            data[1] = USB_DT_STRING;
            return get_desc(*(usb_string_desc*)data, index);

        case USB_DT_BOS:
            data[0] = sizeof(usb_bos_desc);
            data[1] = USB_DT_BOS;
            return get_desc(*(usb_bos_desc*)data);

        default:
            log_error("unsupported descriptor: %s", usb_desc_str(type));
            return USB_RESULT_STALL;
        }
    }

    case USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_CONFIGURATION: {
        log_debug("get_configuration");
        return get_configuration(data[0]);
    }

    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_CONFIGURATION: {
        log_debug("set_configuration(%hu)", val & 0xff);
        return set_configuration(val & 0xff);
    }

    case USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ISOCH_DELAY: {
        log_debug("set isoc delay %hu", val);
        return USB_RESULT_SUCCESS;
    }

    default:
        log_error("unknown request 0x%04hx val:0x%04hx idx:0x%04hx %zu bytes",
                  req, val, idx, length);
        return USB_RESULT_STALL;
    }
}

usb_result device::handle_ep0(usb_packet& p) {
    if (m_stalled && p.token != USB_TOKEN_SETUP)
        return USB_RESULT_STALL;

    switch (p.token) {
    case USB_TOKEN_SETUP: {
        if (p.length != 8)
            return USB_RESULT_STALL;

        m_stalled = false;
        m_ep0.req = p.data[0] | (u16)p.data[1] << 8;
        m_ep0.val = p.data[2] | (u16)p.data[3] << 8;
        m_ep0.idx = p.data[4] | (u16)p.data[5] << 8;
        m_ep0.len = p.data[6] | (u16)p.data[7] << 8;
        m_ep0.pos = 0;

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
        return USB_RESULT_SUCCESS;
    }

    case USB_TOKEN_IN: {
        switch (m_ep0.state) {
        case STATE_DATA: {
            if (!(m_ep0.req & USB_REQ_IN))
                return USB_RESULT_STALL;

            size_t length = min<size_t>(p.length, m_ep0.len - m_ep0.pos);
            memcpy(p.data, m_ep0.buf + m_ep0.pos, length);
            m_ep0.pos += length;

            if (m_ep0.pos >= m_ep0.len)
                m_ep0.state = STATE_STATUS;
            return USB_RESULT_SUCCESS;
        }

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
            log_error("received input token without prior setup");
            return USB_RESULT_SUCCESS;
        }
    }

    case USB_TOKEN_OUT: {
        switch (m_ep0.state) {
        case STATE_DATA: {
            if (m_ep0.req & USB_REQ_IN)
                return USB_RESULT_STALL;

            size_t length = min<size_t>(p.length, m_ep0.len - m_ep0.pos);
            memcpy(m_ep0.buf + m_ep0.pos, p.data, length);
            m_ep0.pos += length;

            if (m_ep0.pos >= m_ep0.len)
                m_ep0.state = STATE_STATUS;
            return USB_RESULT_SUCCESS;
        }

        case STATE_STATUS: {
            m_ep0.state = STATE_SETUP;
            return USB_RESULT_SUCCESS;
        }

        default:
            log_error("USB_TOKEN_OUT: not in data state!");
            return USB_RESULT_SUCCESS;
        }
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
        if (failed(p)) {
            m_ep0.state = STATE_SETUP;
            m_stalled = true;
        }

        return;
    }

    log_info("[-] %s (TODO)", to_string(p).c_str());
}

} // namespace usb
} // namespace vcml
