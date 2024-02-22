/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/usb_types.h"

namespace vcml {

const char* usb_speed_str(usb_speed speed) {
    switch (speed) {
    case USB_SPEED_LOW:
        return "USB_SPEED_LOW";
    case USB_SPEED_FULL:
        return "USB_SPEED_FULL";
    case USB_SPEED_HIGH:
        return "USB_SPEED_HIGH";
    case USB_SPEED_SUPER:
        return "USB_SPEED_SUPER";
    case USB_SPEED_NONE:
    default:
        return "USB_SPEED_NONE";
    }
}

const char* usb_token_str(usb_token token) {
    switch (token) {
    case USB_TOKEN_IN:
        return "USB_TOKEN_IN";
    case USB_TOKEN_OUT:
        return "USB_TOKEN_OUT";
    case USB_TOKEN_SETUP:
        return "USB_TOKEN_SETUP";
    default:
        return "USB_TOKEN_UNKNOWN";
    }
}

const char* usb_result_str(usb_result res) {
    switch (res) {
    case USB_RESULT_SUCCESS:
        return "USB_RESULT_SUCCESS";
    case USB_RESULT_INCOMPLETE:
        return "USB_RESULT_INCOMPLETE";
    case USB_RESULT_NODEV:
        return "USB_RESULT_NODEV";
    case USB_RESULT_NACK:
        return "USB_RESULT_NACK";
    case USB_RESULT_STALL:
        return "USB_RESULT_STALL";
    case USB_RESULT_BABBLE:
        return "USB_RESULT_BABBLE";
    case USB_RESULT_IOERROR:
        return "USB_RESULT_IOERROR";
    default:
        return "USB_RESULT_INVALID";
    }
};

ostream& operator<<(ostream& os, const usb_packet& p) {
    os << mkstr("%s @ %u.%u [", usb_token_str(p.token), p.addr, p.epno);

    if (p.length == 0)
        os << "<no data>";
    else {
        for (size_t i = 0; i < p.length - 1; i++)
            os << mkstr("%02hhx ", p.data[i]);
        os << mkstr("%02hhx", p.data[p.length - 1]);
    }

    os << "] (" << usb_result_str(p.result) << ")";
    return os;
}

usb_packet usb_packet_setup(u32 addr, void* data, size_t len) {
    usb_packet pkt;
    pkt.addr = addr;
    pkt.epno = 0;
    pkt.data = (u8*)data;
    pkt.length = len;
    pkt.token = USB_TOKEN_SETUP;
    pkt.result = USB_RESULT_INCOMPLETE;
    return pkt;
}

usb_packet usb_packet_out(u32 addr, u32 epno, const void* data, size_t len) {
    usb_packet pkt;
    pkt.addr = addr;
    pkt.epno = epno;
    pkt.data = (u8*)data;
    pkt.length = len;
    pkt.token = USB_TOKEN_OUT;
    pkt.result = USB_RESULT_INCOMPLETE;
    return pkt;
}

usb_packet usb_packet_in(u32 addr, u32 epno, void* data, size_t len) {
    usb_packet pkt;
    pkt.addr = addr;
    pkt.epno = epno;
    pkt.data = (u8*)data;
    pkt.length = len;
    pkt.token = USB_TOKEN_IN;
    pkt.result = USB_RESULT_INCOMPLETE;
    return pkt;
}

const char* usb_desc_str(u8 dt) {
    switch (dt) {
    case USB_DT_DEVICE:
        return "USB_DT_DEVICE";
    case USB_DT_CONFIG:
        return "USB_DT_CONFIG";
    case USB_DT_STRING:
        return "USB_DT_STRING";
    case USB_DT_INTERFACE:
        return "USB_DT_INTERFACE";
    case USB_DT_ENDPOINT:
        return "USB_DT_ENDPOINT";
    case USB_DT_DEVICE_QUALIFIER:
        return "USB_DT_DEVICE_QUALIFIER";
    case USB_DT_OTHER_SPEED_CONFIG:
        return "USB_DT_OTHER_SPEED_CONFIG";
    case USB_DT_DEBUG:
        return "USB_DT_DEBUG";
    case USB_DT_INTERFACE_ASSOC:
        return "USB_DT_INTERFACE_ASSOC";
    case USB_DT_BOS:
        return "USB_DT_BOS";
    case USB_DT_DEVICE_CAPABILITY:
        return "USB_DT_DEVICE_CAPABILITY";
    case USB_DT_HID:
        return "USB_DT_HID";
    case USB_DT_REPORT:
        return "USB_DT_REPORT";
    case USB_DT_PHYSICAL:
        return "USB_DT_PHYSICAL";
    case USB_DT_CS_INTERFACE:
        return "USB_DT_CS_INTERFACE";
    case USB_DT_CS_ENDPOINT:
        return "USB_DT_CS_ENDPOINT";
    case USB_DT_ENDPOINT_COMPANION:
        return "USB_DT_ENDPOINT_COMPANION";
    default:
        return "USB_DT_UNKOWN";
    }
}

const char* usb_endpoint_str(u8 type) {
    switch (type & 3) {
    case USB_EP_ISOC:
        return "USB_EP_ISOC";
    case USB_EP_BULK:
        return "USB_EP_BULK";
    case USB_EP_IRQ:
        return "USB_EP_IRQ";
    case USB_EP_CTRL:
    default:
        return "USB_EP_CTRL";
    }
}

} // namespace vcml
