/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/clk.h"
#include "vcml/protocols/sd.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/i2c.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/eth.h"
#include "vcml/protocols/can.h"
#include "vcml/protocols/usb.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/virtio.h"

#include "vcml/tracing/tracer_inscight.h"

namespace vcml {

#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_TRANSACTION_TRACE_FW) && \
    defined(INSCIGHT_TRANSACTION_TRACE_BW)

static inscight::protocol_kind inscight_protocol(protocol_kind kind) {
    switch (kind) {
    case PROTO_TLM:
        return inscight::PROTO_TLM;
    case PROTO_GPIO:
        return inscight::PROTO_GPIO;
    case PROTO_CLK:
        return inscight::PROTO_CLK;
    case PROTO_PCI:
        return inscight::PROTO_PCI;
    case PROTO_I2C:
        return inscight::PROTO_I2C;
    case PROTO_SPI:
        return inscight::PROTO_SPI;
    case PROTO_SD:
        return inscight::PROTO_SD;
    case PROTO_SERIAL:
        return inscight::PROTO_SERIAL;
    case PROTO_VIRTIO:
        return inscight::PROTO_VIRTIO;
    case PROTO_ETHERNET:
        return inscight::PROTO_ETHERNET;
    case PROTO_CAN:
        return inscight::PROTO_CAN;
    case PROTO_USB:
        return inscight::PROTO_USB;
    default:
        return inscight::PROTO_UNKNOWN;
    }
}

template <typename PAYLOAD>
string serialize(const PAYLOAD& tx) {
    return "{}"; // default empty
}

template <>
string serialize(const tlm_generic_payload& tx) {
    ostringstream os;

    os << "{";
    os << "\"address\":" << tx.get_address() << ",";
    os << "\"data\":[";
    unsigned char* data_ptr = tx.get_data_ptr();
    for (unsigned int i = 0; i < tx.get_data_length(); ++i) {
        os << (u32)data_ptr[i];
        if (i < tx.get_data_length() - 1)
            os << ",";
    }
    os << "],";

    os << "\"command\":";
    switch (tx.get_command()) {
    case TLM_READ_COMMAND:
        os << "\"READ\",";
        break;
    case TLM_WRITE_COMMAND:
        os << "\"WRITE\",";
        break;
    case TLM_IGNORE_COMMAND:
        os << "\"IGNORE\",";
        break;
    default:
        os << "\"UNKNOWN\",";
        break;
    }

    os << "\"byte_enable\":[";
    unsigned char* byte_enable_ptr = tx.get_byte_enable_ptr();
    for (unsigned int i = 0; i < tx.get_byte_enable_length(); ++i) {
        os << (u32)byte_enable_ptr[i];
        if (i < tx.get_byte_enable_length() - 1)
            os << ",";
    }
    os << "],";
    os << "\"streaming_width\":\"" << tx.get_streaming_width() << "\",";
    os << "\"dmi_allowed\":" << (tx.is_dmi_allowed() ? "true" : "false")
       << ",";
    os << "\"response_status\":\"" << tx.get_response_string() << "\"";

    if (tx_has_sbi(tx)) {
        os << ",\"sbi\":{";
        os << "\"is_debug\":" << (tx_is_debug(tx) ? "true" : "false") << ",";
        os << "\"is_nodmi\":" << (tx_is_nodmi(tx) ? "true" : "false") << ",";
        os << "\"is_sync\":" << (tx_is_sync(tx) ? "true" : "false") << ",";
        os << "\"is_insn\":" << (tx_is_insn(tx) ? "true" : "false") << ",";
        os << "\"is_excl\":" << (tx_is_excl(tx) ? "true" : "false") << ",";
        os << "\"is_lock\":" << (tx_is_lock(tx) ? "true" : "false") << ",";
        os << "\"is_secure\":" << (tx_is_secure(tx) ? "true" : "false") << ",";

        os << std::dec;
        os << "\"cpuid\":" << tx_cpuid(tx) << ",";
        os << "\"privilege\":" << tx_privilege(tx) << ",";
        os << "\"asid\":" << tx_asid(tx);
        os << "}";
    }

    os << "}";
    return os.str();
}

template <>
string serialize(const gpio_payload& tx) {
    ostringstream os;
    os << "{\"state\":" << (tx.state ? "true" : "false");

    if (tx.vector != GPIO_NO_VECTOR)
        os << ",\"vector\":" << tx.vector;

    os << "}";
    return os.str();
}

template <>
string serialize(const clk_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"newhz\":" << tx.newhz << ",";
    os << "\"oldhz\":" << tx.oldhz;
    os << "}";
    return os.str();
}

template <>
string serialize(const pci_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"command\":\"" << pci_command_str(tx.command) << "\",";
    os << "\"response\":\"" << pci_response_str(tx.response) << "\",";
    os << "\"address_space\":\"" << pci_address_space_str(tx.space) << "\",";
    os << "\"address\":" << tx.addr << ",";

    os << "\"data\":[";
    if (tx.size > 0) {
        u32 i, size = min<u32>(tx.size, sizeof(tx.data));
        for (i = 0; i < size - 1; i++)
            os << ((tx.data >> (i * 8)) & 0xff) << ",";
        os << ((tx.data >> (i * 8)) & 0xff);
    }
    os << "],";

    os << "\"debug\":" << (tx.debug ? "true" : "false");
    os << "}";
    return os.str();
}

template <>
string serialize(const i2c_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"command\":\"" << i2c_command_str(tx.cmd) << "\",";
    os << "\"response\":\"" << i2c_response_str(tx.resp) << "\",";
    os << "\"data\":" << (int)tx.data;
    os << "}";
    return os.str();
}

template <>
string serialize(const spi_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"miso\":" << (int)tx.miso << ",";
    os << "\"mosi\":" << (int)tx.mosi;
    os << "}";
    return os.str();
}

template <>
string serialize(const sd_command& tx) {
    ostringstream os;
    os << "{";

    if (tx.appcmd)
        os << "\"command\":\"SD_APP_CMD\",";
    else
        os << "\"command\":\"SD_CMD\",";

    os << "\"opcode\":\"" << sd_opcode_str(tx.opcode, tx.appcmd) << "\",";
    os << "\"argument\":" << tx.argument << ",";
    os << "\"crc\":" << (int)tx.crc << ",";
    os << "\"spi\":" << (tx.spi ? "true" : "false") << ",";
    os << "\"response\":[";

    size_t len = min<size_t>(tx.resp_len, MWR_ARRAY_SIZE(tx.response));
    if (len > 0) {
        for (size_t i = 0; i < len - 1; i++)
            os << (int)tx.response[i] << ",";
        os << (int)tx.response[len - 1];
    }

    os << "],";
    os << "\"status:\":\"" << sd_status_str(tx.status) << "\"";
    os << "}";
    return os.str();
}

template <>
string serialize(const sd_data& tx) {
    ostringstream os;
    os << "{";

    switch (tx.mode) {
    case SD_READ:
        os << "\"command\":\"SD_DATA_READ\",";
        if (success(tx))
            os << "\"data\":" << (int)tx.data << ",";
        os << "\"status\":\"" << sd_status_str(tx.status.read) << "\"";
        break;

    case SD_WRITE:
        os << "\"command\":\"SD_DATA_WRITE\",";
        os << "\"data\":" << (int)tx.data << ",";
        os << "\"status\":\"" << sd_status_str(tx.status.write) << "\"";
        break;

    default:
        break;
    }

    os << "}";
    return os.str();
}

template <>
string serialize(const vq_message& tx) {
    ostringstream os;
    os << "{";

    os << "\"index\":" << tx.index << ",";

    os << "\"input_buffers\":[";
    for (size_t i = 0; i < tx.in.size(); i++) {
        os << "{";
        os << "\"addr\":" << tx.in[i].addr << ",\"size\":" << tx.in[i].size;
        os << "}";
        if (i < tx.in.size() - 1)
            os << ",";
    }
    os << "],";

    os << "\"output_buffers\":[";
    for (size_t i = 0; i < tx.out.size(); i++) {
        os << "{";
        os << "\"addr\":" << tx.out[i].addr << ",\"size\":" << tx.out[i].size;
        os << "}";
        if (i < tx.out.size() - 1)
            os << ",";
    }
    os << "],";

    os << "\"status\":\"" << virtio_status_str(tx.status) << "\"";

    os << "}";
    return os.str();
}

template <>
string serialize(const serial_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"data\":" << (tx.data & tx.mask) << ",";
    os << "\"bits\":" << tx.width << ",";
    os << "\"baud\":" << tx.baud << ",";
    os << "\"parity\":\"" << serial_parity_str(tx.parity) << "\",";
    os << "\"stop\":\"" << serial_stop_str(tx.stop) << "\"";
    os << "}";
    return os.str();
}

template <>
string serialize(const eth_frame& tx) {
    ostringstream os;
    os << "{";

    os << "\"type\":\"" << tx.identify() << "\"";

    if (!tx.valid()) {
        os << "}";
        return os.str();
    }

    os << ",";

    os << "\"sourceaddr\":\"" << tx.source() << "\",";
    os << "\"destaddr\":\"" << tx.destination() << "\",";

    os << "\"data\":[";
    for (size_t i = 0; i < tx.size(); i++) {
        os << (int)tx[i];
        if (i < tx.size() - 1)
            os << ",";
    }
    os << "]";

    os << "}";
    return os.str();
}

template <>
string serialize(const can_frame& tx) {
    ostringstream os;
    os << "{";

    if (tx.is_fdf())
        os << "\"type\":\"CAN_FD\",";
    else
        os << "\"type\":\"CAN\",";

    os << "\"flags\":" << (int)tx.flags << ",";
    os << "\"rtr\":" << (tx.is_rtr() ? "true" : "false") << ",";
    os << "\"err\":" << (tx.is_err() ? "true" : "false") << ",";
    os << "\"brs\":" << (tx.is_brs() ? "true" : "false") << ",";
    os << "\"esi\":" << (tx.is_esi() ? "true" : "false") << ",";

    os << "\"data\":[";
    for (size_t i = 0; i < tx.length(); i++) {
        os << (int)tx.data[i];
        if (i < tx.length() - 1)
            os << ",";
    }
    os << "]";

    os << "}";
    return os.str();
}

template <>
string serialize(const usb_packet& tx) {
    ostringstream os;
    os << "{";
    os << "\"token\":\"" << usb_token_str(tx.token) << "\",";
    os << "\"addr\":" << tx.addr << ",";
    os << "\"endpoint\":" << tx.epno << ",";
    os << "\"data\":[";
    for (size_t i = 0; i < tx.length; i++) {
        os << (int)tx.data[i];
        if (i < tx.length - 1)
            os << ",";
    }
    os << "],";
    os << "\"status\":\"" << usb_result_str(tx.result) << "\"";
    os << "}";
    return os.str();
}

template <typename PAYLOAD>
void tracer_inscight::do_trace(const activity<PAYLOAD>& msg) {
    auto json = serialize(msg.payload);
    auto kind = inscight_protocol(msg.kind);
    auto time = time_to_ps(msg.t);
    if (is_backward_trace(msg.dir)) {
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        INSCIGHT_TRANSACTION_TRACE_BW(msg.port, time, kind, json.c_str());
    } else {
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        INSCIGHT_TRANSACTION_TRACE_FW(msg.port, time, kind, json.c_str());
    }
}

#else

template <typename PAYLOAD>
void tracer_inscight::do_trace(const activity<PAYLOAD>& msg) {
    // nothing to do
}

#endif

void tracer_inscight::trace(const activity<tlm_generic_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<gpio_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<clk_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<pci_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<i2c_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<spi_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<sd_command>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<sd_data>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<vq_message>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<serial_payload>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<eth_frame>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<can_frame>& msg) {
    do_trace(msg);
}

void tracer_inscight::trace(const activity<usb_packet>& msg) {
    do_trace(msg);
}

tracer_inscight::tracer_inscight(): tracer() {
#if !defined(HAVE_INSCIGHT) || !defined(INSCIGHT_TRANSACTION_TRACE_FW) || \
    !defined(INSCIGHT_TRANSACTION_TRACE_BW)
    log_warn("InSCight tracing not available");
#endif
}

tracer_inscight::~tracer_inscight() {
    // nothing to do
}

} // namespace vcml
