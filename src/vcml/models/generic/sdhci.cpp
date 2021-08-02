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

#include "vcml/models/generic/sdhci.h"

namespace vcml { namespace generic {

    u8 sdhci::calc_crc7() const {
        u8 buffer[5] = {
            (u8)(m_cmd.opcode | 0x40),
            (u8)(m_cmd.argument >> 24),
            (u8)(m_cmd.argument >> 16),
            (u8)(m_cmd.argument >>  8),
            (u8)(m_cmd.argument >>  0),
        };
        return crc7(buffer, sizeof(buffer)) | 1;
    }

    void sdhci::reset_response(int response_reg_nr) {
        RESPONSE[response_reg_nr] = 0x00000000;
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
            RESPONSE[resp_reg_nr] |= m_cmd.response[i] << (shift_count * 8);
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
            PRESENT_STATE &= state; // clear the PRESENT_STATE bit

            switch (state) {
            case ~COMMAND_INHIBIT_CMD:
                NORMAL_INT_STAT |= INT_COMMAND_COMPLETE;
                break;

            case ~COMMAND_INHIBIT_DAT:
                NORMAL_INT_STAT |= INT_TRANSFER_COMPLETE;
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
                NORMAL_INT_STAT &= ~INT_BUFFER_WRITE_READY;
                break;

            case ~BUFFER_READ_ENABLE:
                NORMAL_INT_STAT &= ~INT_BUFFER_READ_READY;
                break;
            }

        } else {
            PRESENT_STATE |= state; // set the PRESENT_STATE bit

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
                NORMAL_INT_STAT |= INT_BUFFER_WRITE_READY;
                break;

            case BUFFER_READ_ENABLE:
                NORMAL_INT_STAT |= INT_BUFFER_READ_READY;
                break;
            }
        }
    }

    void sdhci::transfer_data_from_sd() {
        u16 sd_bufpos = 0;

        while (sd_bufpos <= ((BLOCK_SIZE & 0x0fff) + 2)) {
            switch (SD_OUT.read_data(m_buffer[sd_bufpos++])) {
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

        while (sd_bufpos <= ((BLOCK_SIZE & 0x0fff) + 2)) {
            switch (SD_OUT.write_data(m_buffer[sd_bufpos++])) {
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
            m_buffer[m_bufptr++] = (BUFFER_DATA_PORT >> 8 * i) & 0xff;
    }

    void sdhci::transfer_data_to_port() {
        BUFFER_DATA_PORT = 0x00000000;
        for(unsigned int i = 0; i < 4; i++)
            BUFFER_DATA_PORT |= m_buffer[m_bufptr++] << 8 * i;
    }

    u16 sdhci::write_CMD(u16 val) {
        set_present_state(COMMAND_INHIBIT_CMD);

        m_cmd.spi = false;
        m_cmd.appcmd = false;
        m_cmd.opcode = (val & 0x3f00) >> 8;
        m_cmd.argument = ARG;
        m_cmd.crc = calc_crc7();
        m_cmd.resp_len = 0;
        m_cmd.status = SD_INCOMPLETE;

        if (m_cmd.opcode == 17 || m_cmd.opcode == 18)
            set_present_state(READ_TRANSFER_ACTIVE);

        if (m_cmd.opcode == 24 || m_cmd.opcode == 25)
            set_present_state(WRITE_TRANSFER_ACTIVE);

        SD_OUT->sd_transport(m_cmd);
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
            NORMAL_INT_STAT &= ~INT_COMMAND_COMPLETE;
            ERROR_INT_STAT |= ERR_CMD_CRC;
            break;

        case SD_ERR_ARG:
            NORMAL_INT_STAT &= ~INT_COMMAND_COMPLETE;
            ERROR_INT_STAT |= ERR_DATA_END_BIT;
            break;

        case SD_ERR_ILLEGAL:
            NORMAL_INT_STAT &= ~INT_COMMAND_COMPLETE;
            NORMAL_INT_STAT |= INT_ERROR;
            ERROR_INT_STAT |= ERR_CMD_TIMEOUT;
            break;

        default:
             VCML_ERROR("invalid sd_status %d", m_cmd.status);
        }

        if ((val & 0x0001) && (val & 0x0002))
            NORMAL_INT_STAT |= INT_TRANSFER_COMPLETE;

        IRQ.write(true);
        return val;
    }

    u32 sdhci::read_BUFFER_DATA_PORT() {
        if (!(PRESENT_STATE & BUFFER_READ_ENABLE)) {
            log_warn("reading BUFFER_DATA_PORT not allowed");
            return BUFFER_DATA_PORT;
        }

        transfer_data_to_port();

        u32 blksz = BLOCK_SIZE & 0x0fff;
        if (m_bufptr >= blksz) { // block is completely read
            set_present_state(~BUFFER_READ_ENABLE);
            BLOCK_COUNT_16BIT -= 1;
            m_bufptr = 0;

            if (BLOCK_COUNT_16BIT == 0) { // all the data blocks are read
                set_present_state(~READ_TRANSFER_ACTIVE);
                IRQ.write(true);
                return BUFFER_DATA_PORT;
            }

            transfer_data_from_sd();
            set_present_state(BUFFER_READ_ENABLE);
        }

        return BUFFER_DATA_PORT;
    }

    u32 sdhci::write_BUFFER_DATA_PORT(u32 val) {
        if (!(PRESENT_STATE & BUFFER_WRITE_ENABLE)) {
            log_warn("writing BUFFER_DATA_PORT not allowed");
            return BUFFER_DATA_PORT;
        }

        BUFFER_DATA_PORT = val;
        transfer_data_from_port();

        u32 blksz = BLOCK_SIZE & 0x0fff;
        if (m_bufptr >= blksz) { // block is completely written
            set_present_state(~BUFFER_WRITE_ENABLE);
            BLOCK_COUNT_16BIT -= 1;
            m_bufptr = 0;

            u16 crc = crc16(m_buffer, (BLOCK_SIZE & 0x0fff));
            m_buffer[blksz + 0] = (u8)(crc >> 8);
            m_buffer[blksz + 1] = (u8)(crc >> 0);

            transfer_data_to_sd();

            if (BLOCK_COUNT_16BIT == 0) { // all the data blocks are written
                set_present_state(~WRITE_TRANSFER_ACTIVE);
                IRQ.write(true);
                return val;
            }

            set_present_state(BUFFER_WRITE_ENABLE);
        }

        return val;
    }

    u16 sdhci::write_CLOCK_CTRL(u16 val) {
        CLOCK_CTRL = val;

        // show the driver that the internal clock is stable: see SD Host
        // Controller Simplified Specification page 68
        if (val)
            CLOCK_CTRL |= 0x0002;

        return CLOCK_CTRL;
    }

    u8 sdhci::write_SOFTWARE_RESET(u8 val) {
        switch (val) {
        case RESET_ALL:
            reset();
            break;

        case RESET_CMD_LINE:
            PRESENT_STATE   &= ~0x00000001;
            NORMAL_INT_STAT &= ~0x0001;
            break;

        case RESET_DAT_LINE:
            PRESENT_STATE   &= ~0x00000F06;
            NORMAL_INT_STAT &= ~0x003E;
            break;

        default:
            VCML_ERROR("invalid reset identifier %d", val);
        }

        return val & SOFTWARE_RESET;
    }

    u16 sdhci::write_NORMAL_INT_STAT(u16 val) {
        IRQ.write(false);
        return NORMAL_INT_STAT & ~val; // RW1C functionality
    }

    u16 sdhci::write_ERROR_INT_STAT(u16 val) {
        // if all the errors are handled, write zero to the error interrupt bit
        // of the NORMAL_INT_STAT register
        if (!(ERROR_INT_STAT & ~val)) {
            NORMAL_INT_STAT &= ~0x8000;
            if (!NORMAL_INT_STAT)
                IRQ.write(false);
        }

        return ERROR_INT_STAT & ~val; // RW1C functionality
    }

    u32 sdhci::read_CAPABILITIES() {
        u32 capabilities = CAPABILITIES;
        if (dma_enabled)
            return (capabilities | 0x00400000);
        return capabilities;
    }

    void sdhci::dma_thread() {
        u32 boundary = 0;
        tlm_response_status rs;

        while (true) {
            wait(m_dma_start);

            // normally the boundary should be 512K-12 bytes, i.e. 524276 bytes
            boundary = (4096 << ((BLOCK_SIZE & 0x7000) >> 12)) - 12;

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
            NORMAL_INT_STAT |= INT_TRANSFER_COMPLETE;
            IRQ.write(true);
        }
    }

    tlm_response_status sdhci::dma_read(u32 boundary) {
        u32 offset = 0;
        u32 blksz = BLOCK_SIZE & 0xfff;
        tlm_response_status rs;

        while (true) {
            if (offset + blksz >= boundary) {
                // this never happens with Linux...
                VCML_ERROR("SDMA boundary exceeded, not implemented");
            }

            rs = OUT.write(SDMA_SYSTEM_ADDRESS, m_buffer, blksz);
            if (failed(rs))
                break;

            SDMA_SYSTEM_ADDRESS += blksz;
            offset += blksz;

            BLOCK_COUNT_16BIT -= 1;
            if (BLOCK_COUNT_16BIT == 0) {
                offset = 0;
                break;
            }

            transfer_data_from_sd();
        }

        return rs;
    }

    tlm_response_status sdhci::dma_write(u32 boundary) {
        u32 offset = 0;
        u32 blksz = BLOCK_SIZE & 0xfff;
        u16 crc;
        tlm_response_status rs;

        while (true) {
            if (offset + blksz >= boundary) {
                // this is never happens with Linux...
                VCML_ERROR("SDMA boundary exceeded, not implemented");
            }

            rs = OUT.read(SDMA_SYSTEM_ADDRESS, m_buffer, blksz);
            if (failed(rs))
                break;

            SDMA_SYSTEM_ADDRESS += blksz;
            offset += blksz;

            crc = crc16(m_buffer, blksz);
            m_buffer[blksz + 0] = (u8)(crc >> 8);
            m_buffer[blksz + 1] = (u8)(crc >> 0);

            transfer_data_to_sd();

            BLOCK_COUNT_16BIT -= 1;
            if (BLOCK_COUNT_16BIT == 0) {
                offset = 0;
                break;
            }
        }

        return rs;
    }

    sdhci::sdhci(const sc_module_name& nm):
        peripheral(nm),
        m_cmd(),
        m_bufptr(0),
        m_dma_start("dma_start"),
        SDMA_SYSTEM_ADDRESS     ("SDMA_SYSTEM_ADDRESS",     0x000, 0x00000000),
        BLOCK_SIZE              ("BLOCK_SIZE",              0x004, 0x0000),
        BLOCK_COUNT_16BIT       ("BLOCK_COUNT_16BIT",       0x006, 0x0000),
        ARG                     ("ARG",                     0x008, 0x00000000),
        TRANSFER_MODE           ("TRANSFER_MODE",           0x00c, 0x0000),
        CMD                     ("CMD",                     0x00e, 0x0000),
        RESPONSE                ("RESPONSE",                0X010, 0X00),
        BUFFER_DATA_PORT        ("BUFFER_DATA_PORT",        0x020, 0x00000000),
        PRESENT_STATE           ("PRESENT_STATE",           0x024, CARD_INSERTED),
        HOST_CONTROL_1          ("HOST_CONTROL_1",          0x028, 0x00),
        POWER_CTRL              ("POWER_CTRL",              0x029, 0x0E),
        CLOCK_CTRL              ("CLOCK_CTRL",              0x02c, 0x0000),
        TIMEOUT_CTRL            ("TIMEOUT_CTRL",            0x02e, 0x00),
        SOFTWARE_RESET          ("SOFTWARE_RESET",          0x02f, 0x00),
        NORMAL_INT_STAT         ("NORMAL_INT_STAT",         0x030, 0x0000),
        ERROR_INT_STAT          ("ERROR_INT_STAT",          0x032, 0x0000),
        NORMAL_INT_STAT_ENABLE  ("NORMAL_INT_STAT_ENABLE",  0x034, 0x0000),
        ERROR_INT_STAT_ENABLE   ("ERROR_INT_STAT_ENABLE",   0x036, 0x0000),
        NORMAL_INT_SIG_ENABLE   ("NORMAL_INT_SIG_ENABLE",   0x038, 0x0000),
        ERROR_INT_SIG_ENABLE    ("ERROR_INT_SIG_ENABLE",    0x03a, 0x0000),
        CAPABILITIES            ("CAPABILITIES",            0x040, 0x00000000),
        MAX_CURR_CAP            ("MAX_CURR_CAP",            0x048, 0x00000001),
        HOST_CONTROLLER_VERSION ("HOST_CONTROLLER_VERSION", 0x0fe, 0x0000),
        F_SDH30_AHB_CONFIG      ("F_SDH30_AHB_CONFIG",      0x100, 0x00),
        F_SDH30_ESD_CONTROL     ("F_SDH30_ESD_CONTROL",     0x124, 0x00),
        dma_enabled("dma_enabled", true),
        IRQ("IRQ"),
        IN("IN"),
        OUT("OUT"),
        SD_OUT("SD_OUT") {

        SDMA_SYSTEM_ADDRESS.sync_on_write();
        SDMA_SYSTEM_ADDRESS.allow_read_write();

        BLOCK_SIZE.sync_never();
        BLOCK_SIZE.allow_read_write();

        BLOCK_COUNT_16BIT.sync_never();
        BLOCK_COUNT_16BIT.allow_read_write();

        ARG.sync_never();
        ARG.allow_read_write();

        TRANSFER_MODE.sync_never();
        TRANSFER_MODE.allow_read_write();

        CMD.sync_on_write();
        CMD.allow_read_write();
        CMD.write = &sdhci::write_CMD;

        RESPONSE.sync_never();
        RESPONSE.allow_read_only();

        BUFFER_DATA_PORT.sync_always();
        BUFFER_DATA_PORT.allow_read_write();
        BUFFER_DATA_PORT.read = &sdhci::read_BUFFER_DATA_PORT;
        BUFFER_DATA_PORT.write = &sdhci::write_BUFFER_DATA_PORT;

        PRESENT_STATE.sync_never();
        PRESENT_STATE.allow_read_only();

        HOST_CONTROL_1.sync_never();
        HOST_CONTROL_1.allow_read_write();

        POWER_CTRL.sync_never();
        POWER_CTRL.allow_read_write();

        CLOCK_CTRL.sync_on_write();
        CLOCK_CTRL.allow_read_write();
        CLOCK_CTRL.write = &sdhci::write_CLOCK_CTRL;

        TIMEOUT_CTRL.sync_never();
        TIMEOUT_CTRL.allow_read_write();

        SOFTWARE_RESET.sync_on_write();
        SOFTWARE_RESET.allow_read_write();
        SOFTWARE_RESET.write = &sdhci::write_SOFTWARE_RESET;

        NORMAL_INT_STAT.sync_on_write();
        NORMAL_INT_STAT.allow_read_write();
        NORMAL_INT_STAT.write = &sdhci::write_NORMAL_INT_STAT;

        ERROR_INT_STAT.sync_on_write();
        ERROR_INT_STAT.allow_read_write();
        ERROR_INT_STAT.write = &sdhci::write_ERROR_INT_STAT;

        NORMAL_INT_STAT_ENABLE.sync_never();
        NORMAL_INT_STAT_ENABLE.allow_read_write();

        ERROR_INT_STAT_ENABLE.sync_never();
        ERROR_INT_STAT_ENABLE.allow_read_write();

        NORMAL_INT_SIG_ENABLE.sync_never();
        NORMAL_INT_SIG_ENABLE.allow_read_write();

        ERROR_INT_SIG_ENABLE.sync_never();
        ERROR_INT_SIG_ENABLE.allow_read_write();

        CAPABILITIES.sync_always();
        CAPABILITIES.allow_read_only();
        CAPABILITIES.read = &sdhci::read_CAPABILITIES;
        CAPABILITIES[0] = CAPABILITY_VALUES_0;

        MAX_CURR_CAP.sync_never();
        MAX_CURR_CAP.allow_read_only();

        HOST_CONTROLLER_VERSION.sync_never();
        HOST_CONTROLLER_VERSION.allow_read_only();

        F_SDH30_AHB_CONFIG.sync_never();
        F_SDH30_AHB_CONFIG.allow_read_write();

        F_SDH30_ESD_CONTROL.sync_never();
        F_SDH30_ESD_CONTROL.allow_read_write();

        SC_HAS_PROCESS(sdhci);
        SC_THREAD(dma_thread);
    }

    sdhci::~sdhci() {
        // nothing to do
    }

    void sdhci::reset() {
        peripheral::reset();
        CAPABILITIES[0] = CAPABILITY_VALUES_0;
    }

}}
