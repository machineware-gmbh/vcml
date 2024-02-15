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

const char* usb_descriptor_type_str(u8 dt) {
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

} // namespace vcml
