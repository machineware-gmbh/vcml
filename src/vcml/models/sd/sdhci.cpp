/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/sd/sdhci.h"

namespace vcml {
namespace sd {

void sdhci::reset_response(int response_reg_nr) {
    response[response_reg_nr] = 0x00000000;
}

void sdhci::store_response() {
    // for different kinds of responses the card status has to be written
    // in different bytes of the RESPONSE registers, see
    // SD Host Controller Simplified Specification page 48 and Physical
    // Layer Specifications page 98
    int resp_reg_nr = 0;
    int shift_count = 3;

    // SD response R1b or R1 for AUTO CMD12 or AUTO CMD23 (CMD23 is not
    // relevant for controller version 1.0)
    if (m_cmd.opcode == 12 || m_cmd.opcode == 23)
        resp_reg_nr = 3; // bits 127:96 of the RESPONSE register

    if (m_cmd.resp_len == 17) {
        resp_reg_nr = 3;
        shift_count = 2;
        for (int i = 0; i < 3; i++)
            reset_response(i);
    }

    reset_response(resp_reg_nr);

    for (unsigned int i = 1; i < m_cmd.resp_len - 1; i++) {
        // first write the lowest response byte (response[1]), (response[0]
        // and response[5] just contain CMD number and CRC bits)
        response[resp_reg_nr] |= m_cmd.response[i] << (shift_count * 8);
        shift_count--;

        // if we have to shift by 32 bits, just write in the next RESPONSE
        // register (just the case with R2)
        if (shift_count == -1) {
            shift_count = 3;
            resp_reg_nr--;
        }
    }
}

void sdhci::set_present_state(unsigned int state) {
    if (state > (1 << 16)) {
        present_state &= state; // clear the PRESENT_STATE bit

        switch (state) {
        case ~COMMAND_INHIBIT_CMD:
            normal_int_stat |= INT_COMMAND_COMPLETE;
            break;

        case ~COMMAND_INHIBIT_DAT:
            normal_int_stat |= INT_TRANSFER_COMPLETE;
            break;

        case ~DAT_LINE_ACTIVE:
            set_present_state(~COMMAND_INHIBIT_DAT);
            break;

        case ~READ_TRANSFER_ACTIVE:
            set_present_state(~DAT_LINE_ACTIVE);
            break;

        case ~WRITE_TRANSFER_ACTIVE:
            set_present_state(~DAT_LINE_ACTIVE);
            break;

        case ~BUFFER_WRITE_ENABLE:
            normal_int_stat &= ~INT_BUFFER_WRITE_READY;
            break;

        case ~BUFFER_READ_ENABLE:
            normal_int_stat &= ~INT_BUFFER_READ_READY;
            break;
        }

    } else {
        present_state |= state; // set the PRESENT_STATE bit

        switch (state) {
        case DAT_LINE_ACTIVE:
            set_present_state(COMMAND_INHIBIT_DAT);
            break;

        case READ_TRANSFER_ACTIVE:
            set_present_state(DAT_LINE_ACTIVE);
            break;

        case WRITE_TRANSFER_ACTIVE:
            set_present_state(DAT_LINE_ACTIVE);
            break;

        case BUFFER_WRITE_ENABLE:
            normal_int_stat |= INT_BUFFER_WRITE_READY;
            break;

        case BUFFER_READ_ENABLE:
            normal_int_stat |= INT_BUFFER_READ_READY;
            break;
        }
    }
}

void sdhci::transfer_data_from_sd() {
    u16 sd_bufpos = 0;

    while (sd_bufpos <= ((block_size & 0x0fff) + 2)) {
        switch (sd_out.read_data(m_buffer[sd_bufpos++])) {
        case SDTX_OK:
            break;

        case SDTX_OK_BLK_DONE:
            // checking the CRC of the data block is not necessary
            return;

        case SDTX_OK_COMPLETE:
            return;

        default:
            VCML_ERROR("card returned status error");
        }
    }

    VCML_ERROR("no SDTX_OK_BLK_DONE or SDTX_OK_COMPLETE received");
}

void sdhci::transfer_data_to_sd() {
    u16 sd_bufpos = 0;

    while (sd_bufpos <= ((block_size & 0x0fff) + 2)) {
        switch (sd_out.write_data(m_buffer[sd_bufpos++])) {
        case SDRX_OK:
            break;

        case SDRX_OK_BLK_DONE:
            return;

        case SDRX_OK_COMPLETE:
            return;

        case SDRX_ERR_CRC:
            VCML_ERROR("SDRX_ERR_CRC");
            return;

        case SDRX_ERR_INT:
            VCML_ERROR("SDRX_ERR_INTERNAL");
            return;

        case SDRX_ERR_ILLEGAL:
            VCML_ERROR("SDRX_ERR_ILLEGAL");
            return;

        default:
            VCML_ERROR("card returned status error");
        }
    }

    VCML_ERROR("no SDRX_OK_BLK_DONE or SDRX_OK_COMPLETE received");
}

void sdhci::transfer_data_from_port() {
    for (unsigned int i = 0; i < 4; i++)
        m_buffer[m_bufptr++] = (buffer_data_port >> 8 * i) & 0xff;
}

void sdhci::transfer_data_to_port() {
    buffer_data_port = 0x00000000;
    for (unsigned int i = 0; i < 4; i++)
        buffer_data_port |= m_buffer[m_bufptr++] << 8 * i;
}

void sdhci::write_cmd(u16 val) {
    set_present_state(COMMAND_INHIBIT_CMD);

    m_cmd.spi = false;
    m_cmd.appcmd = false;
    m_cmd.opcode = (val & 0x3f00) >> 8;
    m_cmd.argument = arg;
    m_cmd.crc = sd_crc7(m_cmd);
    m_cmd.resp_len = 0;
    m_cmd.status = SD_INCOMPLETE;

    if (m_cmd.opcode == 17 || m_cmd.opcode == 18)
        set_present_state(READ_TRANSFER_ACTIVE);

    if (m_cmd.opcode == 24 || m_cmd.opcode == 25)
        set_present_state(WRITE_TRANSFER_ACTIVE);

    sd_out->sd_transport(m_cmd);
    store_response();
    set_present_state(~COMMAND_INHIBIT_CMD);

    switch (m_cmd.status) {
    case SD_OK:
        break;

    case SD_OK_TX_RDY:
        transfer_data_from_sd();
        if (!dma_enabled) {
            set_present_state(BUFFER_READ_ENABLE);
        } else {
            set_present_state(DAT_LINE_ACTIVE);
            m_dma_start.notify(SC_ZERO_TIME);
        }
        break;

    case SD_OK_RX_RDY:
        if (!dma_enabled) {
            set_present_state(BUFFER_WRITE_ENABLE);
        } else {
            set_present_state(DAT_LINE_ACTIVE);
            m_dma_start.notify(SC_ZERO_TIME);
        }
        break;

    case SD_ERR_CRC:
        normal_int_stat &= ~INT_COMMAND_COMPLETE;
        error_int_stat |= ERR_CMD_CRC;
        break;

    case SD_ERR_ARG:
        normal_int_stat &= ~INT_COMMAND_COMPLETE;
        error_int_stat |= ERR_DATA_END_BIT;
        break;

    case SD_ERR_ILLEGAL:
        normal_int_stat &= ~INT_COMMAND_COMPLETE;
        normal_int_stat |= INT_ERROR;
        error_int_stat |= ERR_CMD_TIMEOUT;
        break;

    default:
        VCML_ERROR("invalid sd_status %d", m_cmd.status);
    }

    if ((val & 0x0001) && (val & 0x0002))
        normal_int_stat |= INT_TRANSFER_COMPLETE;

    irq.write(true);
    cmd = val;
}

u32 sdhci::read_buffer_data_port() {
    if (!(present_state & BUFFER_READ_ENABLE)) {
        log_warn("reading BUFFER_DATA_PORT not allowed");
        return buffer_data_port;
    }

    transfer_data_to_port();

    u32 blksz = block_size & 0x0fff;
    if (m_bufptr >= blksz) { // block is completely read
        set_present_state(~BUFFER_READ_ENABLE);
        block_count_16_bit -= 1;
        m_bufptr = 0;

        if (block_count_16_bit == 0) { // all the data blocks are read
            set_present_state(~READ_TRANSFER_ACTIVE);
            irq.write(true);
            return buffer_data_port;
        }

        transfer_data_from_sd();
        set_present_state(BUFFER_READ_ENABLE);
    }

    return buffer_data_port;
}

void sdhci::write_buffer_data_port(u32 val) {
    if (!(present_state & BUFFER_WRITE_ENABLE)) {
        log_warn("writing BUFFER_DATA_PORT not allowed");
        return;
    }

    buffer_data_port = val;
    transfer_data_from_port();

    u32 blksz = block_size & 0x0fff;
    if (m_bufptr >= blksz) { // block is completely written
        set_present_state(~BUFFER_WRITE_ENABLE);
        block_count_16_bit -= 1;
        m_bufptr = 0;

        u16 crc = crc16(m_buffer, (block_size & 0x0fff));
        m_buffer[blksz + 0] = (u8)(crc >> 8);
        m_buffer[blksz + 1] = (u8)(crc >> 0);

        transfer_data_to_sd();

        if (block_count_16_bit == 0) { // all the data blocks are written
            set_present_state(~WRITE_TRANSFER_ACTIVE);
            irq.write(true);
            buffer_data_port = val;
            return;
        }

        set_present_state(BUFFER_WRITE_ENABLE);
    }

    buffer_data_port = val;
}

void sdhci::write_clock_ctrl(u16 val) {
    clock_ctrl = val;

    // show the driver that the internal clock is stable: see SD Host
    // Controller Simplified Specification page 68
    if (val)
        clock_ctrl |= 0x0002;
}

void sdhci::write_software_reset(u8 val) {
    switch (val) {
    case RESET_ALL:
        reset();
        break;

    case RESET_CMD_LINE:
        present_state &= ~0x00000001;
        normal_int_stat &= ~0x0001;
        break;

    case RESET_DAT_LINE:
        present_state &= ~0x00000F06;
        normal_int_stat &= ~0x003E;
        break;

    default:
        VCML_ERROR("invalid reset identifier %d", val);
    }

    software_reset &= val;
}

void sdhci::write_normal_int_stat(u16 val) {
    irq.write(false);
    normal_int_stat &= ~val; // RW1C functionality
}

void sdhci::write_error_int_stat(u16 val) {
    // if all the errors are handled, write zero to the error interrupt bit
    // of the NORMAL_INT_STAT register
    if (!(error_int_stat & ~val)) {
        normal_int_stat &= ~0x8000;
        if (!normal_int_stat)
            irq.write(false);
    }

    error_int_stat &= ~val; // RW1C functionality
}

u32 sdhci::read_capabilities() {
    u32 caps = capabilities;
    return dma_enabled ? (caps | 0x00400000) : (caps & ~0x00400000);
}

void sdhci::dma_thread() {
    u32 boundary = 0;
    tlm_response_status rs;

    while (true) {
        wait(m_dma_start);

        // normally the boundary should be 512K-12 bytes, i.e. 524276 bytes
        boundary = (4096 << ((block_size & 0x7000) >> 12)) - 12;

        if (m_cmd.status == SD_OK_TX_RDY) {
            rs = dma_read(boundary);
        } else if (m_cmd.status == SD_OK_RX_RDY) {
            rs = dma_write(boundary);
        } else {
            VCML_ERROR("illegal state for DMA command");
        }

        if (failed(rs))
            log_warn("DMA failed: %s", tlm_response_to_str(rs));

        set_present_state(~DAT_LINE_ACTIVE);
        normal_int_stat |= INT_TRANSFER_COMPLETE;
        irq.write(true);
    }
}

tlm_response_status sdhci::dma_read(u32 boundary) {
    u32 offset = 0;
    u32 blksz = block_size & 0xfff;
    tlm_response_status rs;

    while (true) {
        if (offset + blksz >= boundary) {
            // this never happens with Linux...
            VCML_ERROR("SDMA boundary exceeded, not implemented");
        }

        rs = out.write(sdma_system_address, m_buffer, blksz);
        if (failed(rs))
            break;

        sdma_system_address += blksz;
        offset += blksz;

        block_count_16_bit -= 1;
        if (block_count_16_bit == 0)
            break;

        transfer_data_from_sd();
    }

    return rs;
}

tlm_response_status sdhci::dma_write(u32 boundary) {
    u32 offset = 0;
    u32 blksz = block_size & 0xfff;
    u16 crc;
    tlm_response_status rs;

    while (true) {
        if (offset + blksz >= boundary) {
            // this is never happens with Linux...
            VCML_ERROR("SDMA boundary exceeded, not implemented");
        }

        rs = out.read(sdma_system_address, m_buffer, blksz);
        if (failed(rs))
            break;

        sdma_system_address += blksz;
        offset += blksz;

        crc = crc16(m_buffer, blksz);
        m_buffer[blksz + 0] = (u8)(crc >> 8);
        m_buffer[blksz + 1] = (u8)(crc >> 0);

        transfer_data_to_sd();

        block_count_16_bit -= 1;
        if (block_count_16_bit == 0)
            break;
    }

    return rs;
}

sdhci::sdhci(const sc_module_name& nm):
    peripheral(nm),
    m_cmd(),
    m_bufptr(0),
    m_dma_start("dma_start"),
    sdma_system_address("sdma_system_address", 0x000, 0x00000000),
    block_size("block_size", 0x004, 0x0000),
    block_count_16_bit("block_count_16_bit", 0x006, 0x0000),
    arg("arg", 0x008, 0x00000000),
    transfer_mode("transfer_mode", 0x00c, 0x0000),
    cmd("cmd", 0x00e, 0x0000),
    response("response", 0X010, 0X00),
    buffer_data_port("buffer_data_port", 0x020, 0x00000000),
    present_state("present_state", 0x024, CARD_INSERTED),
    host_control_1("host_control_1", 0x028, 0x00),
    power_ctrl("power_ctrl", 0x029, 0x0E),
    clock_ctrl("clock_ctrl", 0x02c, 0x0000),
    timeout_ctrl("timeout_ctrl", 0x02e, 0x00),
    software_reset("software_reset", 0x02f, 0x00),
    normal_int_stat("normal_int_stat", 0x030, 0x0000),
    error_int_stat("error_int_stat", 0x032, 0x0000),
    normal_int_stat_enable("normal_int_stat_enable", 0x034, 0x0000),
    error_int_stat_enable("error_int_stat_enable", 0x036, 0x0000),
    normal_int_sig_enable("normal_int_sig_enable", 0x038, 0x0000),
    error_int_sig_enable("error_int_sig_enable", 0x03a, 0x0000),
    capabilities("capabilities", 0x040, 0x00000000),
    max_curr_cap("max_curr_cap", 0x048, 0x00000001),
    host_controller_version("host_controller_version", 0x0fe, 0x0000),
    f_sd_h30_ahb_config("f_sd_h30_ahb_config", 0x100, 0x00),
    f_sd_h30_esd_control("f_sd_h30_esd_control", 0x124, 0x00),
    dma_enabled("dma_enabled", true),
    irq("irq"),
    in("in"),
    out("out"),
    sd_out("sd_out") {
    sdma_system_address.sync_on_write();
    sdma_system_address.allow_read_write();

    block_size.sync_never();
    block_size.allow_read_write();

    block_count_16_bit.sync_never();
    block_count_16_bit.allow_read_write();

    arg.sync_never();
    arg.allow_read_write();

    transfer_mode.sync_never();
    transfer_mode.allow_read_write();

    cmd.sync_on_write();
    cmd.allow_read_write();
    cmd.on_write(&sdhci::write_cmd);

    response.sync_never();
    response.allow_read_only();

    buffer_data_port.sync_always();
    buffer_data_port.allow_read_write();
    buffer_data_port.on_read(&sdhci::read_buffer_data_port);
    buffer_data_port.on_write(&sdhci::write_buffer_data_port);

    present_state.sync_never();
    present_state.allow_read_only();

    host_control_1.sync_never();
    host_control_1.allow_read_write();

    power_ctrl.sync_never();
    power_ctrl.allow_read_write();

    clock_ctrl.sync_on_write();
    clock_ctrl.allow_read_write();
    clock_ctrl.on_write(&sdhci::write_clock_ctrl);

    timeout_ctrl.sync_never();
    timeout_ctrl.allow_read_write();

    software_reset.sync_on_write();
    software_reset.allow_read_write();
    software_reset.on_write(&sdhci::write_software_reset);

    normal_int_stat.sync_on_write();
    normal_int_stat.allow_read_write();
    normal_int_stat.on_write(&sdhci::write_normal_int_stat);

    error_int_stat.sync_on_write();
    error_int_stat.allow_read_write();
    error_int_stat.on_write(&sdhci::write_error_int_stat);

    normal_int_stat_enable.sync_never();
    normal_int_stat_enable.allow_read_write();

    error_int_stat_enable.sync_never();
    error_int_stat_enable.allow_read_write();

    normal_int_sig_enable.sync_never();
    normal_int_sig_enable.allow_read_write();

    error_int_sig_enable.sync_never();
    error_int_sig_enable.allow_read_write();

    capabilities.sync_always();
    capabilities.allow_read_only();
    capabilities.on_read(&sdhci::read_capabilities);
    capabilities[0] = CAPABILITY_VALUES_0;

    max_curr_cap.sync_never();
    max_curr_cap.allow_read_only();

    host_controller_version.sync_never();
    host_controller_version.allow_read_only();

    f_sd_h30_ahb_config.sync_never();
    f_sd_h30_ahb_config.allow_read_write();

    f_sd_h30_esd_control.sync_never();
    f_sd_h30_esd_control.allow_read_write();

    SC_HAS_PROCESS(sdhci);
    SC_THREAD(dma_thread);
}

sdhci::~sdhci() {
    // nothing to do
}

void sdhci::reset() {
    peripheral::reset();
    capabilities[0] = CAPABILITY_VALUES_0;
}

VCML_EXPORT_MODEL(vcml::sd::sdhci, name, args) {
    return new sdhci(name);
}

} // namespace sd
} // namespace vcml
