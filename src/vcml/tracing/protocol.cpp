/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/tracing/protocol.h"

#include "vcml/core/systemc.h"
#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/clk.h"
#include "vcml/protocols/sd.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/i2c.h"
#include "vcml/protocols/lin.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/eth.h"
#include "vcml/protocols/can.h"
#include "vcml/protocols/usb.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/signal.h"
#include "vcml/protocols/virtio.h"

namespace vcml {

string trace_payload_to_json(const tlm_generic_payload& tx) {
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

string trace_payload_to_json(const gpio_payload& tx) {
    ostringstream os;
    os << "{\"state\":" << (tx.state ? "true" : "false");

    if (tx.vector != GPIO_NO_VECTOR)
        os << ",\"vector\":" << tx.vector;

    os << "}";
    return os.str();
}

string trace_payload_to_json(const clk_desc& tx) {
    ostringstream os;
    os << "{";
    os << "\"period\":" << time_to_ns(tx.period) << ",";
    os << "\"polarity\":\"" << (tx.polarity ? "pos" : "neg") << "edge\",";
    os << "\"duty_cycle\":" << tx.duty_cycle;
    os << "}";
    return os.str();
}

string trace_payload_to_json(const pci_payload& tx) {
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

string trace_payload_to_json(const i2c_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"command\":\"" << i2c_command_str(tx.cmd) << "\",";
    os << "\"response\":\"" << i2c_response_str(tx.resp) << "\",";
    os << "\"data\":" << (int)tx.data;
    os << "}";
    return os.str();
}

string trace_payload_to_json(const lin_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"linid\":\"" << (int)tx.linid << "\",";
    os << "\"data\":[";
    if (tx.size() > 0) {
        for (size_t i = 0; i < tx.size() - 1; i++)
            os << (int)tx.data[i] << ",";
        os << (int)tx.data[tx.size() - 1];
    }
    os << "],";

    os << "\"status\":\"" << lin_status_str(tx.status) << "\",";
    os << "}";
    return os.str();
}

string trace_payload_to_json(const spi_payload& tx) {
    ostringstream os;
    os << "{";
    os << "\"miso\":" << (int)tx.miso << ",";
    os << "\"mosi\":" << (int)tx.mosi;
    os << "}";
    return os.str();
}

string trace_payload_to_json(const sd_command& tx) {
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

string trace_payload_to_json(const sd_data& tx) {
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

string trace_payload_to_json(const vq_message& tx) {
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

string trace_payload_to_json(const serial_payload& tx) {
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

string trace_payload_to_json(const signal_payload_base& tx) {
    ostringstream os;
    os << "{";
    os << "\"data\":" << tx.to_string();
    os << "}";
    return os.str();
}

string trace_payload_to_json(const eth_frame& tx) {
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

string trace_payload_to_json(const can_frame& tx) {
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

string trace_payload_to_json(const usb_packet& tx) {
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

} // namespace vcml
