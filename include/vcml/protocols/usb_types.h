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

usb_packet usb_packet_setup(u32 addr, void* data, size_t len);
usb_packet usb_packet_out(u32 addr, u32 epno, const void* data, size_t len);
usb_packet usb_packet_in(u32 addr, u32 epno, void* data, size_t len);

constexpr bool success(const usb_packet& p) {
    return success(p.result);
}

constexpr bool failed(const usb_packet& p) {
    return failed(p.result);
}

enum usb_request : u16 {
    USB_REQ_DEVICE = 0x0000,
    USB_REQ_IFACE = 0x0100,
    USB_REQ_ENDPOINT = 0x0200,
    USB_REQ_OTHER = 0x0300,

    USB_REQ_CLASS = 0x2000,
    USB_REQ_VENDOR = 0x4000,

    USB_REQ_OUT = 0x0000,
    USB_REQ_IN = 0x8000,

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
    USB_DT_HID = 0x21,
    USB_DT_REPORT = 0x22,
    USB_DT_PHYSICAL = 0x23,
    USB_DT_CS_INTERFACE = 0x24,
    USB_DT_CS_ENDPOINT = 0x25,
    USB_DT_ENDPOINT_COMPANION = 0x30,
};

const char* usb_desc_str(u8 dt);

enum usb_cfg_attrs : u8 {
    USB_CFG_BATTERY = bit(4),
    USB_CFG_WAKEUP = bit(5),
    USB_CFG_SELF_POWERED = bit(6),
    USB_CFG_ONE = bit(7),
};

enum usb_device_class : u8 {
    USB_CLASS_RESERVED = 0x00,
    USB_CLASS_AUDIO = 0x01,
    USB_CLASS_COMM = 0x02,
    USB_CLASS_HID = 0x03,
    USB_CLASS_PHYSICAL = 0x05,
    USB_CLASS_STILL_IMAGE = 0x06,
    USB_CLASS_PRINTER = 0x07,
    USB_CLASS_MASS_STORAGE = 0x08,
    USB_CLASS_HUB = 0x09,
    USB_CLASS_CDC_DATA = 0x0a,
    USB_CLASS_SMART_CARD = 0x0b,
    USB_CLASS_CONTENT_SECURITY = 0x0d,
    USB_CLASS_VIDEO = 0x0e,
    USB_CLASS_PERSONAL_HEALTHCARE = 0x0f,
    USB_CLASS_AUDIO_VIDEO_DEVICES = 0x10,
    USB_CLASS_BILLBOARD = 0x11,
    USB_CLASS_TYPE_C_BRIDGE = 0x12,
    USB_CLASS_APP_SPECIFIC = 0xfe,
    USB_CLASS_VENDOR_SPECIFIC = 0xff,
};

enum usb_endpoint_bits : u8 {
    USB_EP_CTRL = 0,
    USB_EP_ISOC = 1,
    USB_EP_BULK = 2,
    USB_EP_IRQ = 3,
};

const char* usb_endpoint_str(u8 type);

constexpr u8 usb_ep_in(u8 addr) {
    return 0x80 | addr;
}

constexpr u8 usb_ep_out(u8 addr) {
    return addr;
}

#pragma pack(push, 1)

struct usb_device_desc {
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
    u8 manufacturer_strid;
    u8 product_strid;
    u8 serial_number_strid;
    u8 num_configurations;
};

struct usb_config_desc {
    u8 length;
    u8 descriptor_type;
    u16 total_length;
    u8 num_interfaces;
    u8 configuration_value;
    u8 configuration_strid;
    u8 attributes;
    u8 max_power;
};

struct usb_string_desc {
    u8 length;
    u8 descriptor_type;
    u16 wstring[];
};

struct usb_interface_desc {
    u8 length;
    u8 descriptor_type;
    u8 interface_number;
    u8 alternate_setting;
    u8 num_endpoints;
    u8 interface_class;
    u8 interface_subclass;
    u8 interface_protocol;
    u8 interface_strid;
};

struct usb_endpoint_desc {
    u8 length;
    u8 descriptor_type;
    u8 endpoint_address;
    u8 attributes;
    u16 max_packet_size;
    u8 interval;
    u8 refresh;
    u8 sync_address;
};

struct usb_bos_desc {
    u8 length;
    u8 descriptor_type;
    u16 total_length;
    u8 num_device_caps;
};

#pragma pack(pop)

} // namespace vcml

#endif
