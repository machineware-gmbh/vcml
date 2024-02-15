/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_USB_TYPES_H
#define VCML_PROTOCOLS_USB_TYPES_H

#include "vcml/core/types.h"

namespace vcml {

enum usb_speed {
    USB_SPEED_NONE = -1,
    USB_SPEED_LOW = 0,
    USB_SPEED_FULL = 1,
    USB_SPEED_HIGH = 2,
    USB_SPEED_SUPER = 3,
};

const char* usb_speed_str(usb_speed speed);

enum usb_token {
    USB_TOKEN_IN = 0x69,
    USB_TOKEN_OUT = 0xe1,
    USB_TOKEN_SETUP = 0x2d,
};

const char* usb_token_str(usb_token token);

enum usb_result {
    USB_RESULT_SUCCESS = 1,
    USB_RESULT_INCOMPLETE = 0,
    USB_RESULT_NODEV = -1,
    USB_RESULT_NACK = -2,
    USB_RESULT_STALL = -3,
    USB_RESULT_BABBLE = -4,
    USB_RESULT_IOERROR = -5,
};

const char* usb_result_str(usb_result res);

constexpr bool success(usb_result result) {
    return result > USB_RESULT_INCOMPLETE;
}

constexpr bool failed(usb_result result) {
    return result < USB_RESULT_INCOMPLETE;
}

struct usb_packet {
    u32 addr;
    u32 epno;

    usb_token token;
    usb_result result;

    u8* data;
    size_t length;
};

ostream& operator<<(ostream& os, const usb_packet& p);

constexpr bool success(const usb_packet& p) {
    return success(p.result);
}

constexpr bool failed(const usb_packet& p) {
    return failed(p.result);
}

enum usb_request : u32 {
    USB_REQ_GET_STATUS = 0x00,
    USB_REQ_CLEAR_FEATURE = 0x01,
    USB_REQ_SET_FEATURE = 0x03,
    USB_REQ_SET_ADDRESS = 0x05,
    USB_REQ_GET_DESCRIPTOR = 0x06,
    USB_REQ_SET_DESCRIPTOR = 0x07,
    USB_REQ_GET_CONFIGURATION = 0x08,
    USB_REQ_SET_CONFIGURATION = 0x09,
    USB_REQ_GET_INTERFACE = 0x0a,
    USB_REQ_SET_INTERFACE = 0x0b,
    USB_REQ_SYNCH_FRAME = 0x0c,
    USB_REQ_SET_SEL = 0x30,
    USB_REQ_SET_ISOCH_DELAY = 0x31,

    USB_REQ_DEVICE = 0x00 << 8,
    USB_REQ_INTERFACE = 0x01 << 8,
    USB_REQ_ENDPOINT = 0x02 << 8,
    USB_REQ_OTHER = 0x03 << 8,

    USB_REQ_OUT = 0x0 << 12,
    USB_REQ_IN = 0x8 << 12,
};

enum usb_descriptor_type : u8 {
    USB_DT_DEVICE = 0x01,
    USB_DT_CONFIG = 0x02,
    USB_DT_STRING = 0x03,
    USB_DT_INTERFACE = 0x04,
    USB_DT_ENDPOINT = 0x05,
    USB_DT_DEVICE_QUALIFIER = 0x06,
    USB_DT_OTHER_SPEED_CONFIG = 0x07,
    USB_DT_DEBUG = 0x0a,
    USB_DT_INTERFACE_ASSOC = 0x0b,
    USB_DT_BOS = 0x0f,
    USB_DT_DEVICE_CAPABILITY = 0x10,
    USB_DT_CS_INTERFACE = 0x24,
    USB_DT_CS_ENDPOINT = 0x25,
    USB_DT_ENDPOINT_COMPANION = 0x30,
};

const char* usb_descriptor_type_str(u8 dt);

#pragma pack(push, 1)

struct usb_device_descriptor {
    u8 length;
    u8 descriptor_type;

    u16 bcd_usb;
    u8 device_class;
    u8 device_subclass;
    u8 device_protocol;
    u8 max_packet_size0;
    u16 vendor_id;
    u16 product_id;
    u16 bcd_device;
    u8 manufacturer;
    u8 product;
    u8 serial_number;
    u8 num_configurations;
};

struct usb_config_descriptor {
    u8 length;
    u8 descriptor_type;
    u16 total_length;
    u8 num_interfaces;
    u8 configuration_value;
    u8 configuration;
    u8 attributes;
    u8 max_power;
};

struct usb_string_descriptor {
    u8 length;
    u8 descriptor_type;
    u16 wstring[];
};

struct usb_interface_descriptor {
    u8 length;
    u8 descriptor_type;
    u8 interface_number;
    u8 alternate_setting;
    u8 num_endpoints;
    u8 interface_class;
    u8 interface_subclass;
    u8 interface_protocol;
    u8 interface;
};

struct usb_endpoint_descriptor {
    u8 length;
    u8 descriptor_type;
    u8 endpoint_address;
    u8 attributes;
    u16 max_packet_size;
    u8 interval;
    u8 refresh;
    u8 sync_address;
};

struct usb_bos_descriptor {
    u8 length;
    u8 descriptor_type;
    u16 total_length;
    u8 num_device_caps;
};

#pragma pack(pop)

} // namespace vcml

#endif
