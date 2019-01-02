/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/sd.h"

namespace vcml {

    sd_initiator_socket::sd_initiator_socket():
        tlm::tlm_base_initiator_socket<1, sd_fw_transport_if,
                                          sd_bw_transport_if, 1,
                                          sc_core::SC_ONE_OR_MORE_BOUND>() {
    }

    sd_initiator_socket::sd_initiator_socket(const char* nm):
        tlm::tlm_base_initiator_socket<1, sd_fw_transport_if,
                                          sd_bw_transport_if, 1,
                                          sc_core::SC_ONE_OR_MORE_BOUND>(nm) {
    }

    sc_core::sc_type_index sd_initiator_socket::get_protocol_types() const {
        return typeid(sd_fw_transport_if);
    }

    sd_target_socket::sd_target_socket():
        tlm::tlm_base_target_socket<1, sd_fw_transport_if,
                                       sd_bw_transport_if, 1,
                                       sc_core::SC_ONE_OR_MORE_BOUND>() {
    }

    sd_target_socket::sd_target_socket(const char* nm):
        tlm::tlm_base_target_socket<1, sd_fw_transport_if,
                                       sd_bw_transport_if, 1,
                                       sc_core::SC_ONE_OR_MORE_BOUND>(nm) {
    }

    sc_core::sc_type_index sd_target_socket::get_protocol_types() const {
        return typeid(sd_bw_transport_if);
    }

    sd_initiator_stub::sd_initiator_stub(const sc_module_name& nm):
        sc_module(nm),
        sd_bw_transport_if(),
        SD_OUT("SD_OUT") {
        SD_OUT.bind(*this);
    }

    sd_initiator_stub::~sd_initiator_stub() {
        /* nothing to do */
    }

    sd_status sd_target_stub::sd_transport(sd_command& tx) {
        log_debug("received SD CMD%hhu (%u)", tx.opcode, tx.argument);
        tx.resp_len = 0;
        return SD_OK;
    }

    sd_tx_status sd_target_stub::sd_data_read(u8& val) {
        log_debug("data read request");
        return SDTX_ERR_ILLEGAL;
    }

    sd_rx_status sd_target_stub::sd_data_write(u8 data) {
        log_debug("data write request [%02hhx]", data);
        return SDRX_OK;
    }

    sd_target_stub::sd_target_stub(const sc_module_name& nm):
        component(nm),
        sd_fw_transport_if(),
        SD_IN("SD_IN") {
        SD_IN.bind(*this);
    }

    sd_target_stub::~sd_target_stub() {
        /* nothing to do */
    }

    static const char* do_cmd_str(u8 opcode) {
        switch (opcode) {
        case  0: return "GO_IDLE_STATE";
        case  1: return "SEND_OP_COND";
        case  2: return "ALL_SEND_CID";
        case  3: return "SEND_RELATIVE_ADDR";
        case  4: return "SET_DSR";
        case  5: return "SD_IO_SEND_OP_COND";
        case  6: return "SWITCH_FUNC";
        case  7: return "SELECT_DESELECT_CARD";
        case  8: return "SEND_IF_COND";
        case  9: return "SEND_CSD";
        case 10: return "SEND_CID";
        case 11: return "VOLTAGE_SWITCH";
        case 12: return "STOP_TRANSMISSION";
        case 13: return "SEND_STATUS";
        case 15: return "GO_INACTIVE_STATE";
        case 16: return "SET_BLOCKLEN";
        case 17: return "READ_SINGLE_BLOCK";
        case 18: return "READ_MULTIPLE_BLOCK";
        case 19: return "SEND_TUNING_BLOCK";
        case 20: return "SPEED_CLASS_CONTROL";
        case 23: return "SET_BLOCK_COUNT";
        case 24: return "WRITE_BLOCK";
        case 25: return "WRITE_MULTIPLE_BLOCK";
        case 26: return "RESERVED_26";
        case 27: return "PROGRAM_CSD";
        case 28: return "SET_WRITE_PROT";
        case 29: return "CLR_WRITE_PROT";
        case 30: return "SEND_WRITE_PROT";
        case 32: return "ERASE_WR_BLK_START_ADDR";
        case 33: return "ERASE_WR_BLK_END_ADDR";
        case 38: return "ERASE";
        case 42: return "LOCK_UNLOCK";
        case 48: return "READ_EXTR_SINGLE";
        case 49: return "WRITE_EXTR_SINGLE";
        case 52: return "IO_RW_DIRECT";
        case 53: return "IO_RW_EXTENDED";
        case 55: return "APP_CMD";
        case 56: return "GEN_CMD";
        case 58: return "READ_OCR";
        case 59: return "CRC_ON_OFF";
        case 60: return "RESERVED_60";
        case 61: return "RESERVED_61";
        case 62: return "RESERVED_62";
        case 63: return "RESERVED_63";
        default: return "UNKNOWN";
        }
    }

    static const char* do_acmd_str(u8 opcode) {
        switch (opcode) {
        case  6: return "SET_BUS_WIDTH";
        case 13: return "SD_STATUS";
        case 22: return "SEND_NUM_WR_BLOCKS";
        case 23: return "SET_WR_BLK_ERASE_COUNT";
        case 41: return "SD_SEND_OP_COND";
        case 42: return "SET_CLR_CARD_DETECT";
        case 51: return "SEND_SCR";
        default: return "UNKNOWN";
        }
    }

    const char* sd_cmd_str(u8 opcode, bool appcmd) {
        return appcmd ? do_acmd_str(opcode) : do_cmd_str(opcode);
    }

#define HEXW(w) std::hex << std::setw(w) << std::setfill('0')

    string sd_cmd_str(const sd_command& tx, bool appcmd) {
        stringstream ss; u8 op = tx.opcode;
        ss << (appcmd ? "ACMD" : "CMD") << std::dec << (unsigned int)tx.opcode;
        ss << " '" << sd_cmd_str(op, appcmd) << "'";
        ss << " (" << HEXW(8) << tx.argument << ")";

        if (tx.resp_len > 0) {
            ss << " resp = { ";
            for (unsigned int i = 0; i < tx.resp_len; i++)
                ss << HEXW(2) << (unsigned int)tx.response[i] << " ";
            ss << "}" << std::dec;
        }

        return ss.str();
    }

}
