/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SD_SDHCI_H
#define VCML_SD_SDHCI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/sd.h"

namespace vcml {
namespace sd {

class sdhci : public peripheral
{
private:
    enum reset_kind {
        RESET_ALL = bit(0),
        RESET_CMD_LINE = bit(1),
        RESET_DAT_LINE = bit(2)
    };

    enum present_state : unsigned int {
        COMMAND_INHIBIT_CMD = bit(0),
        COMMAND_INHIBIT_DAT = bit(1),
        DAT_LINE_ACTIVE = bit(2),
        WRITE_TRANSFER_ACTIVE = bit(8),
        READ_TRANSFER_ACTIVE = bit(9),
        BUFFER_WRITE_ENABLE = bit(10),
        BUFFER_READ_ENABLE = bit(11),
        CARD_INSERTED = bit(16),
    };

    enum normal_interrupts {
        INT_COMMAND_COMPLETE = bit(0),
        INT_TRANSFER_COMPLETE = bit(1),
        INT_DMA_INTERRUPT = bit(3),
        INT_BUFFER_WRITE_READY = bit(4),
        INT_BUFFER_READ_READY = bit(5),
        INT_ERROR = bit(15),
    };

    enum error_interrupts {
        ERR_CMD_TIMEOUT = bit(0),
        ERR_CMD_CRC = bit(1),
        ERR_CMD_END_BIT = bit(2),
        ERR_CMD_INDEX = bit(3),
        ERR_DATA_TIMEOUT = bit(4),
        ERR_DATA_CRC = bit(5),
        ERR_DATA_END_BIT = bit(6),
    };

    enum capabilities : u32 {
        CAPABILITY_VALUES_0 = 0x01000a8a,
    };

    sd_command m_cmd;
    // sd_status  m_status;

    u16 m_bufptr;
    u8 m_buffer[4096];

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

    gpio_initiator_socket irq;
    tlm_target_socket in;
    tlm_initiator_socket out;

    sd_initiator_socket sd_out;

    sdhci() = delete;
    sdhci(const sc_module_name& name);
    virtual ~sdhci();
    VCML_KIND(sd::sdhci);

    virtual void reset() override;
};

} // namespace sd
} // namespace vcml

#endif
