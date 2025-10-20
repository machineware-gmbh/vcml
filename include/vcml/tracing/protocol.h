/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACING_PROTOCOL_H
#define VCML_TRACING_PROTOCOL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

enum trace_protocol_ids : size_t {
    PROTO_TLM,
    PROTO_GPIO,
    PROTO_CLK,
    PROTO_PCI,
    PROTO_I2C,
    PROTO_LIN,
    PROTO_SPI,
    PROTO_SD,
    PROTO_SERIAL,
    PROTO_SIGNAL,
    PROTO_VIRTIO,
    PROTO_ETHERNET,
    PROTO_CAN,
    PROTO_USB,
    // reserved protocol ids
    PROTO_RES0,
    PROTO_RES1,
    PROTO_RES2,
    PROTO_RES3,
    PROTO_RES4,
    PROTO_RES5,
    PROTO_RES6,
    PROTO_RES7,
    PROTO_RES8,
    PROTO_RES9,
    // external protocol ids
    PROTO_EXT0,
    PROTO_EXT1,
    PROTO_EXT2,
    PROTO_EXT3,
    PROTO_EXT4,
    PROTO_EXT5,
    PROTO_EXT6,
    PROTO_EXT7,
    PROTO_EXT8,
    PROTO_EXT9,
};

struct gpio_payload;
struct clk_desc;
struct pci_payload;
struct i2c_payload;
struct lin_payload;
struct spi_payload;
struct sd_command;
struct sd_data;
struct vq_message;
struct serial_payload;
struct signal_payload_base;
struct eth_frame;
struct can_frame;
struct usb_packet;

enum trace_direction : int {
    TRACE_FW = 2,
    TRACE_FW_NOINDENT = 1,
    TRACE_NONE = 0,
    TRACE_BW_NOINDENT = -1,
    TRACE_BW = -2,
};

constexpr bool is_forward_trace(trace_direction dir) {
    return dir > 0;
}

constexpr bool is_backward_trace(trace_direction dir) {
    return dir < 0;
}

string trace_payload_to_json(const tlm_generic_payload& payload);
string trace_payload_to_json(const gpio_payload& payload);
string trace_payload_to_json(const clk_desc& payload);
string trace_payload_to_json(const pci_payload& payload);
string trace_payload_to_json(const i2c_payload& payload);
string trace_payload_to_json(const lin_payload& payload);
string trace_payload_to_json(const spi_payload& payload);
string trace_payload_to_json(const sd_command& payload);
string trace_payload_to_json(const sd_data& payload);
string trace_payload_to_json(const vq_message& payload);
string trace_payload_to_json(const serial_payload& payload);
string trace_payload_to_json(const signal_payload_base& payload);
string trace_payload_to_json(const eth_frame& payload);
string trace_payload_to_json(const can_frame& payload);
string trace_payload_to_json(const usb_packet& payload);

template <typename PAYLOAD>
struct protocol {};

template <>
struct protocol<tlm_generic_payload> {
    static constexpr size_t ID = PROTO_TLM;
    static constexpr const char* NAME = "TLM";
    static constexpr const char* TERMCOLOR = mwr::termcolors::MAGENTA;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const tlm_generic_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<gpio_payload> {
    static constexpr size_t ID = PROTO_GPIO;
    static constexpr const char* NAME = "GPIO";
    static constexpr const char* TERMCOLOR = mwr::termcolors::YELLOW;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const gpio_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<clk_desc> {
    static constexpr size_t ID = PROTO_CLK;
    static constexpr const char* NAME = "CLK";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BLUE;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
    static string to_json(const clk_desc& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<pci_payload> {
    static constexpr size_t ID = PROTO_PCI;
    static constexpr const char* NAME = "PCI";
    static constexpr const char* TERMCOLOR = mwr::termcolors::CYAN;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const pci_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<i2c_payload> {
    static constexpr size_t ID = PROTO_I2C;
    static constexpr const char* NAME = "I2C";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_GREEN;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const i2c_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<lin_payload> {
    static constexpr size_t ID = PROTO_LIN;
    static constexpr const char* NAME = "LIN";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_GREEN;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const lin_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<spi_payload> {
    static constexpr size_t ID = PROTO_SPI;
    static constexpr const char* NAME = "SPI";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_YELLOW;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const spi_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<sd_command> {
    static constexpr size_t ID = PROTO_SD;
    static constexpr const char* NAME = "SD";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_MAGENTA;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const sd_command& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<sd_data> {
    static constexpr size_t ID = protocol<sd_command>::ID;
    static constexpr const char* NAME = protocol<sd_command>::NAME;
    static constexpr const char* TERMCOLOR = protocol<sd_command>::TERMCOLOR;
    static constexpr bool TRACE_FW = protocol<sd_command>::TRACE_FW;
    static constexpr bool TRACE_BW = protocol<sd_command>::TRACE_BW;
    static string to_json(const sd_data& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<serial_payload> {
    static constexpr size_t ID = PROTO_SERIAL;
    static constexpr const char* NAME = "SERIAL";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_RED;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
    static string to_json(const serial_payload& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<signal_payload_base> {
    static constexpr size_t ID = PROTO_SIGNAL;
    static constexpr const char* NAME = "SIGNAL";
    static constexpr const char* TERMCOLOR = mwr::termcolors::RED;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
    static string to_json(const signal_payload_base& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<vq_message> {
    static constexpr size_t ID = PROTO_VIRTIO;
    static constexpr const char* NAME = "VIRTIO";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_CYAN;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const vq_message& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<eth_frame> {
    static constexpr size_t ID = PROTO_ETHERNET;
    static constexpr const char* NAME = "ETHERNET";
    static constexpr const char* TERMCOLOR = mwr::termcolors::BRIGHT_BLUE;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
    static string to_json(const eth_frame& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<can_frame> {
    static constexpr size_t ID = PROTO_CAN;
    static constexpr const char* NAME = "CAN";
    static constexpr const char* TERMCOLOR = mwr::termcolors::RED;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
    static string to_json(const can_frame& payload) {
        return trace_payload_to_json(payload);
    }
};

template <>
struct protocol<usb_packet> {
    static constexpr size_t ID = PROTO_USB;
    static constexpr const char* NAME = "USB";
    static constexpr const char* TERMCOLOR = mwr::termcolors::CYAN;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
    static string to_json(const usb_packet& payload) {
        return trace_payload_to_json(payload);
    }
};

template <typename PAYLOAD>
constexpr trace_direction translate_direction_default(trace_direction dir) {
    switch (dir) {
    case TRACE_FW:
        if (!protocol<PAYLOAD>::TRACE_BW)
            dir = TRACE_FW_NOINDENT;
        // no break
    case TRACE_FW_NOINDENT:
        return protocol<PAYLOAD>::TRACE_FW ? dir : TRACE_NONE;

    case TRACE_BW:
        if (!protocol<PAYLOAD>::TRACE_FW)
            dir = TRACE_BW_NOINDENT;
        // no break
    case TRACE_BW_NOINDENT:
        return protocol<PAYLOAD>::TRACE_BW ? dir : TRACE_NONE;

    default:
        return TRACE_NONE;
    }
}

} // namespace vcml

#endif
