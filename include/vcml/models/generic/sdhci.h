/******************************************************************************
 *                                                                            *
 * Copyright 2020 Lasse Urban, Lukas Juenger, Jan Henrik Weinstock            *
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

#ifndef VCML_GENERIC_SDHCI_H
#define VCML_GENERIC_SDHCI_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/irq.h"
#include "vcml/protocols/sd.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml {
namespace generic {

class sdhci : public peripheral
{
private:
    enum reset_kind {
        RESET_ALL = 1 << 0,
        RESET_CMD_LINE = 1 << 1,
        RESET_DAT_LINE = 1 << 2
    };

    enum present_state : unsigned int {
        COMMAND_INHIBIT_CMD = 1 << 0,
        COMMAND_INHIBIT_DAT = 1 << 1,
        DAT_LINE_ACTIVE = 1 << 2,
        WRITE_TRANSFER_ACTIVE = 1 << 8,
        READ_TRANSFER_ACTIVE = 1 << 9,
        BUFFER_WRITE_ENABLE = 1 << 10,
        BUFFER_READ_ENABLE = 1 << 11,
        CARD_INSERTED = 1 << 16
    };

    enum normal_interrupts {
        INT_COMMAND_COMPLETE = 1 << 0,
        INT_TRANSFER_COMPLETE = 1 << 1,
        INT_DMA_INTERRUPT = 1 << 3,
        INT_BUFFER_WRITE_READY = 1 << 4,
        INT_BUFFER_READ_READY = 1 << 5,
        INT_ERROR = 1 << 15
    };

    enum error_interrupts {
        ERR_CMD_TIMEOUT = 1 << 0,
        ERR_CMD_CRC = 1 << 1,
        ERR_CMD_END_BIT = 1 << 2,
        ERR_CMD_INDEX = 1 << 3,
        ERR_DATA_TIMEOUT = 1 << 4,
        ERR_DATA_CRC = 1 << 5,
        ERR_DATA_END_BIT = 1 << 6
    };

    enum capabilities : u32 {
        CAPABILITY_VALUES_0 = 0x01000a8a,
    };

    sd_command m_cmd;
    // sd_status  m_status;

    u16 m_bufptr;
    u8 m_buffer[514]; // 512 block length + 2 bytes CRC

    u8 calc_crc7() const;

    void reset_response(int response_reg_nr);
    void store_response();
    void set_present_state(unsigned int state);

    void transfer_data_from_sd();
    void transfer_data_to_sd();
    void transfer_data_from_port();
    void transfer_data_to_port();

    void write_cmd(u16 val);
    u32 read_buffer_data_port();
    void write_buffer_data_port(u32 val);
    void write_clock_ctrl(u16 val);
    void write_software_reset(u8 val);
    void write_normal_int_stat(u16 val);
    void write_error_int_stat(u16 val);
    u32 read_capabilities();

    void dma_thread();

    tlm_response_status dma_read(u32 boundary);
    tlm_response_status dma_write(u32 boundary);

    sc_event m_dma_start;

public:
    // Common SDHCI registers
    reg<u32> sdma_system_address;
    reg<u16> block_size;
    reg<u16> block_count_16_bit;

    reg<u32> arg;
    reg<u16> transfer_mode;
    reg<u16> cmd;

    reg<u32, 4> response;

    reg<u32> buffer_data_port;

    reg<u32> present_state;
    reg<u8> host_control_1;
    reg<u8> power_ctrl;
    reg<u16> clock_ctrl;
    reg<u8> timeout_ctrl;
    reg<u8> software_reset;

    reg<u16> normal_int_stat;
    reg<u16> error_int_stat;
    reg<u16> normal_int_stat_enable;
    reg<u16> error_int_stat_enable;
    reg<u16> normal_int_sig_enable;
    reg<u16> error_int_sig_enable;

    reg<u32, 2> capabilities;
    reg<u32> max_curr_cap;

    reg<u16> host_controller_version;

    // Controller specific registers
    reg<u16> f_sd_h30_ahb_config;
    reg<u32> f_sd_h30_esd_control;

    property<bool> dma_enabled;

    irq_initiator_socket irq;
    tlm_target_socket in;
    tlm_initiator_socket out;

    sd_initiator_socket sd_out;

    sdhci() = delete;
    sdhci(const sc_module_name& name);
    virtual ~sdhci();
    VCML_KIND(sdhci);

    virtual void reset() override;
};

} // namespace generic
} // namespace vcml

#endif
