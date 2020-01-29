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

    u8 sdhci::calc_crc7() {
        u8 buffer[5] = {
            (u8)(m_cmd.opcode | 0x40),                          // start bit = 0, transmission bit = 1, see Physical Layer Specifications page 217
            (u8)(m_cmd.argument >> 24),
            (u8)(m_cmd.argument >> 16),
            (u8)(m_cmd.argument >>  8),
            (u8)(m_cmd.argument >>  0),
        };
        return (crc7(buffer, sizeof(buffer)) | 1);              // end bit = 1, see Physical Layer Specifications page 217
    }

    void sdhci::reset_RESPONSE(int response_reg_nr) {
        RESPONSE[response_reg_nr] = 0x00000000;
    }

    void sdhci::write_sd_resp_to_RESPONSE() {
        // for different kinds of responses the card status has to be written in different bytes of the RESPONSE registers
        // see SD Host Controller Simplified Specification page 48 and Physical Layer Specifications page 98

        int response_reg_nr = 0;
        int shift_count = 3;

        if (m_cmd.opcode == 12 || m_cmd.opcode == 23)           // SD response R1b or R1 for AUTO CMD12 or AUTO CMD23 (CMD23 is not relevant for controller version 1.0)
            response_reg_nr = 3;                                // must be written in bits 127:96 of the RESPONSE register

        if (m_cmd.resp_len == 17) {
            response_reg_nr = 3;
            shift_count = 2;
            for(int i = 0; i < 3; i++)
                reset_RESPONSE(i);
        }

        reset_RESPONSE(response_reg_nr);

        for (unsigned int i = 1; i < m_cmd.resp_len - 1; i++) {                     // iterate through the response field
            RESPONSE[response_reg_nr] |= m_cmd.response[i] << (shift_count * 8);    // first write the lowest response byte (response[1]), (response[0] and response[5] just contain CMD number and CRC bits)
            shift_count--;

            if (shift_count == -1) {                                                // if we have to shift by 32 bits, just write in the next RESPONSE register (just the case with R2)
                shift_count = 3;
                response_reg_nr--;
            }
        }
    }

    void sdhci::set_PRESENT_STATE(unsigned int act_state) {
        if (act_state > (1<<16)) {
            PRESENT_STATE &= act_state;                         // clear the PRESENT_STATE bit

            switch (act_state) {
            case ~COMMAND_INHIBIT_CMD:
                NORMAL_INT_STAT |= INT_COMMAND_COMPLETE;
                break;
            case ~COMMAND_INHIBIT_DAT:
                NORMAL_INT_STAT |= INT_TRANSFER_COMPLETE;
                break;
            case ~DAT_LINE_ACTIVE:
                set_PRESENT_STATE(~COMMAND_INHIBIT_DAT);
                break;
            case ~READ_TRANSFER_ACTIVE:
                set_PRESENT_STATE(~DAT_LINE_ACTIVE);
                break;
            case ~WRITE_TRANSFER_ACTIVE:
                set_PRESENT_STATE(~DAT_LINE_ACTIVE);
                break;
            case ~BUFFER_WRITE_ENABLE:
                NORMAL_INT_STAT &= ~INT_BUFFER_WRITE_READY;
                break;
            case ~BUFFER_READ_ENABLE:
                NORMAL_INT_STAT &= ~INT_BUFFER_READ_READY;
                break;
            }

        } else {
            PRESENT_STATE |= act_state;                         // set the PRESENT_STATE bit

            switch (act_state) {
            case DAT_LINE_ACTIVE:
                set_PRESENT_STATE(COMMAND_INHIBIT_DAT);
                break;
            case READ_TRANSFER_ACTIVE:
                set_PRESENT_STATE(DAT_LINE_ACTIVE);
                break;
            case WRITE_TRANSFER_ACTIVE:
                set_PRESENT_STATE(DAT_LINE_ACTIVE);
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

/********************************************************************************
 *                                                                              *
 *                  Transfer data from the SD card to the host (read)           *
 *                                                                              *
 ********************************************************************************/

    void sdhci::transfer_data_from_sd_buffer_to_sdhci_buffer() {
        u16 sd_bufpos = 0;

        while (sd_bufpos <= ((BLOCK_SIZE & 0x0FFF) + 2)) {
            switch (SD_OUT->sd_data_read(m_buffer[sd_bufpos++])) {
            case SDTX_OK:
                break;
            case SDTX_OK_BLK_DONE:
                // checking the CRC of the data block is not necessary in this model
                return;
            case SDTX_OK_COMPLETE:
                return;
            default: VCML_ERROR("card returned status error");
            }
        }
        VCML_ERROR("one block read but got no SDTX_OK_BLK_DONE or SDTX_OK_COMPLETE");
    }

    void sdhci::transfer_data_from_sdhci_buffer_to_BUFFER_DATA_PORT() {
        BUFFER_DATA_PORT = 0x00000000;
        for(unsigned int i = 0; i < 4; i++)
            BUFFER_DATA_PORT |= m_buffer[m_bufptr++] << 8*i;
    }

/********************************************************************************
 *                                                                              *
 *                  Transfer data from the host to the SD card (write)          *
 *                                                                              *
 ********************************************************************************/

    void sdhci::transfer_data_from_BUFFER_DATA_PORT_to_sdhci_buffer() {
        for (unsigned int i = 0; i < 4; i++)
            m_buffer[m_bufptr++] = (BUFFER_DATA_PORT >> 8*i) & 0xFF;
    }

    void sdhci::transfer_data_from_sdhci_buffer_to_sd_buffer() {
        u16 sd_bufpos = 0;

        while (sd_bufpos <= ((BLOCK_SIZE & 0x0FFF) + 2)) {
            switch (SD_OUT->sd_data_write(m_buffer[sd_bufpos++])) {
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
            default: VCML_ERROR("card returned status error");
            }
        }
        VCML_ERROR("one block written but got no SDRX_OK_BLK_DONE or SDRX_OK_COMPLETE");
    }

/********************************************************************************
 *                                                                              *
 *                  Register read and write functions                           *
 *                                                                              *
 ********************************************************************************/

    u32 sdhci::write_SDMA_SYSTEM_ADDRESS(u32 val) {
        SDMA_SYSTEM_ADDRESS = val;
        m_new_DMA_address_event.notify(SC_ZERO_TIME);
        return val;
    }

    u16 sdhci::read_CMD() {
        return CMD;
    }

    // when the host driver writes something in the CMD register (assuming it has already written the registers 000h to 00Ch) the information from the register is read
    // and put into m_cmd. Then, crc7 is calculated and the cmd is send to the sd card.

    u16 sdhci::write_CMD(u16 val) {
        set_PRESENT_STATE(COMMAND_INHIBIT_CMD);

        m_cmd.spi = false;
        m_cmd.opcode = (val & 0x3F00) >> 8;                     // see SD Host Controller Simplified Specification page 45
        m_cmd.argument = ARG;
        m_cmd.crc = calc_crc7();
        m_cmd.resp_len = 0;

        if (m_cmd.opcode == 17 || m_cmd.opcode == 18) {         // read CMD
            set_PRESENT_STATE(READ_TRANSFER_ACTIVE);
        }

        if (m_cmd.opcode == 24 || m_cmd.opcode == 25) {         // write CMD
            set_PRESENT_STATE(WRITE_TRANSFER_ACTIVE);
        }

         m_status = SD_OUT->sd_transport(m_cmd);

        // checking the CRC and the command index is not necessary in this model

        write_sd_resp_to_RESPONSE();

        set_PRESENT_STATE(~COMMAND_INHIBIT_CMD);

        switch (m_status) {
        case SD_OK:
            break;
        case SD_OK_TX_RDY:
            transfer_data_from_sd_buffer_to_sdhci_buffer();
            if (!DMA_enabled) {
                set_PRESENT_STATE(BUFFER_READ_ENABLE);
            } else {
                set_PRESENT_STATE(DAT_LINE_ACTIVE);
                m_start_DMA_event.notify(SC_ZERO_TIME);
            }
            break;
        case SD_OK_RX_RDY:
            if (!DMA_enabled) {
                set_PRESENT_STATE(BUFFER_WRITE_ENABLE);
            } else {
                set_PRESENT_STATE(DAT_LINE_ACTIVE);
                m_start_DMA_event.notify(SC_ZERO_TIME);
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
             VCML_ERROR("invalid sd_status %d", m_status);
        }

        if ((val & 0x0001) && (val & 0x0002))
            NORMAL_INT_STAT |= INT_TRANSFER_COMPLETE;

        IRQ.write(true);
        return val;
    }

    u32 sdhci::read_BUFFER_DATA_PORT() {
        if (PRESENT_STATE & BUFFER_READ_ENABLE) {
            transfer_data_from_sdhci_buffer_to_BUFFER_DATA_PORT();

            if (m_bufptr >= (BLOCK_SIZE & 0x0FFF)) {            // one block is completely read
                set_PRESENT_STATE(~BUFFER_READ_ENABLE);
                BLOCK_COUNT_16BIT -= 1;
                m_bufptr = 0;

                if (BLOCK_COUNT_16BIT == 0) {                   // all the data blocks are read
                    set_PRESENT_STATE(~READ_TRANSFER_ACTIVE);
                    IRQ.write(true);
                    return BUFFER_DATA_PORT;
                }

                transfer_data_from_sd_buffer_to_sdhci_buffer();
                set_PRESENT_STATE(BUFFER_READ_ENABLE);
            }

            return BUFFER_DATA_PORT;
        } else {
            VCML_ERROR("Actually it is not allowed to read data from the BUFFER_DATA_PORT!");
            return 0xffffffff;
        }
    }

    u32 sdhci::write_BUFFER_DATA_PORT(u32 val) {
        if (PRESENT_STATE & BUFFER_WRITE_ENABLE) {
            BUFFER_DATA_PORT = val;
            transfer_data_from_BUFFER_DATA_PORT_to_sdhci_buffer();

            if (m_bufptr >= (BLOCK_SIZE & 0x0FFF)) {            // one block is completely written
                set_PRESENT_STATE(~BUFFER_WRITE_ENABLE);
                BLOCK_COUNT_16BIT -= 1;
                m_bufptr = 0;

                u16 crc = crc16(m_buffer, (BLOCK_SIZE & 0x0FFF));
                m_buffer[(BLOCK_SIZE & 0x0FFF) + 0] = (u8)(crc >> 8);
                m_buffer[(BLOCK_SIZE & 0x0FFF) + 1] = (u8)(crc >> 0);

                transfer_data_from_sdhci_buffer_to_sd_buffer();

                if (BLOCK_COUNT_16BIT == 0) {                   // all the data blocks are written
                    set_PRESENT_STATE(~WRITE_TRANSFER_ACTIVE);
                    IRQ.write(true);
                    return val;
                }

                set_PRESENT_STATE(BUFFER_WRITE_ENABLE);
            }
            return val;
        } else {
            VCML_ERROR("Actually it is not allowed to write data to the BUFFER_DATA_PORT!");
            return 0xffffffff;
        }
    }

    u16 sdhci::read_CLOCK_CTRL() {
        return CLOCK_CTRL;
    }

    u16 sdhci::write_CLOCK_CTRL(u16 val) {
        CLOCK_CTRL = val;
        if (val)
            CLOCK_CTRL |= (0x0002);                             // show the driver that the internal clock is stable: see SD Host Controller Simplified Specification page 68
        return CLOCK_CTRL;
    }

    u8 sdhci::read_SOFTWARE_RESET() {
        return SOFTWARE_RESET;
    }

    u8 sdhci::write_SOFTWARE_RESET(u8 val) {                    // see SD Host Controller Simplified Specification page 74
        switch (val) {
        case RESET_ALL:
            reset();                                            // inherited from class Peripheral: sets all registers to their m_defval from constructor
            CAPABILITIES[0] = CAPABILITY_VALUES_0;              // CAPABILITIES register must be restored
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

    u16 sdhci::read_NORMAL_INT_STAT() {
        return NORMAL_INT_STAT;
    }

    u16 sdhci::write_NORMAL_INT_STAT(u16 val) {
        IRQ.write(false);
        return (NORMAL_INT_STAT & (~val));                      // RW1C functionality
    }

    u16 sdhci::read_ERROR_INT_STAT() {
        return ERROR_INT_STAT;
    }

    u16 sdhci::write_ERROR_INT_STAT(u16 val) {
        if (!(ERROR_INT_STAT & (~val))) {                       // if all the errors are handled, write zero to the error interrupt bit of the NORMAL_INT_STAT register
            NORMAL_INT_STAT &= (~0x8000);
            if (!NORMAL_INT_STAT)
                IRQ.write(false);
        }
        return (ERROR_INT_STAT & (~val));                       // RW1C functionality
    }

    u32 sdhci::read_CAPABILITIES() {
        u32 capabilities = CAPABILITIES;
        if (DMA_enabled)
            return (capabilities | 0x00400000);
        return capabilities;
    }

    void sdhci::exec_DMA_transfer() {
        u32 boundary = 0;
        tlm_response_status rs;

        while (true) {
            wait(m_start_DMA_event);

            boundary = ((4096 << ((BLOCK_SIZE & 0x7000) >> 12)) - 12);   // normally the boundary should be 512K - 12 bytes, i.e. 524276 bytes

            if (m_status == SD_OK_TX_RDY) {
                rs = DMA_read_from_sd(boundary);
            } else if(m_status == SD_OK_RX_RDY) {
                rs = DMA_write_to_sd(boundary);
            } else {
                VCML_ERROR("DMA for this command not permitted!");
            }

            if (failed(rs))
                VCML_ERROR("DMA by the SDHCI controller failed");

            set_PRESENT_STATE(~DAT_LINE_ACTIVE);
            NORMAL_INT_STAT |= INT_TRANSFER_COMPLETE;
            IRQ.write(true);
        }
    }

    tlm_response_status sdhci::DMA_read_from_sd(u32 SDMA_boundary) {
        u32 offset = 0;
        tlm_response_status rs;
        while (true) {
            if (offset+(BLOCK_SIZE & 0x0FFF) >= SDMA_boundary) {                    // expect a boundary exceeding
                // this is never happens with Linux, therefore not implemented
                VCML_ERROR("SDMA boundary of the SDHCI exceeded, not implemented");
            }

            rs = OUT.write(SDMA_SYSTEM_ADDRESS, m_buffer, (BLOCK_SIZE & 0x0FFF));
            if (failed(rs))
                break;
            SDMA_SYSTEM_ADDRESS += (BLOCK_SIZE & 0x0FFF);

            offset += (BLOCK_SIZE & 0x0FFF);
            BLOCK_COUNT_16BIT -= 1;

            if (BLOCK_COUNT_16BIT == 0) {                       // all the data blocks are read
                offset = 0;
                break;
            }

            transfer_data_from_sd_buffer_to_sdhci_buffer();
        }
        return rs;
    }

    tlm_response_status sdhci::DMA_write_to_sd(u32 SDMA_boundary) {
        u32 offset = 0;
        u16 crc;
        tlm_response_status rs;
        while (true) {
            if (offset+(BLOCK_SIZE & 0x0FFF) >= SDMA_boundary) {                    // expect a boundary exceeding
                // this is never happens with Linux, therefore not implemented
                VCML_ERROR("SDMA boundary of the SDHCI exceeded, not implemented");
            }

            rs = OUT.read(SDMA_SYSTEM_ADDRESS, m_buffer, (BLOCK_SIZE & 0x0FFF));
            if (failed(rs))
                break;
            SDMA_SYSTEM_ADDRESS += (BLOCK_SIZE & 0x0FFF);

            offset += (BLOCK_SIZE & 0x0FFF);
            BLOCK_COUNT_16BIT -= 1;

            crc = crc16(m_buffer, (BLOCK_SIZE & 0x0FFF));
            m_buffer[(BLOCK_SIZE & 0x0FFF) + 0] = (u8)(crc >> 8);
            m_buffer[(BLOCK_SIZE & 0x0FFF) + 1] = (u8)(crc >> 0);

            transfer_data_from_sdhci_buffer_to_sd_buffer();

            if (BLOCK_COUNT_16BIT == 0) {                       // all the data blocks are written
                offset = 0;
                break;
            }
        }
        return rs;
    }

    sdhci::sdhci(const sc_module_name& nm):
        peripheral(nm),
        m_cmd(),
        m_status(),
        m_bufptr(0),
        m_start_DMA_event("start_DMA_event"),
        m_new_DMA_address_event("new_DMA_address_event"),
        SDMA_SYSTEM_ADDRESS     ("SDMA_SYSTEM_ADDRESS",     0x000, 0x00000000),
        BLOCK_SIZE              ("BLOCK_SIZE",              0x004, 0x0000),
        BLOCK_COUNT_16BIT       ("BLOCK_COUNT_16BIT",       0x006, 0x0000),
        ARG                     ("ARG",                     0x008, 0x00000000),
        TRANSFER_MODE           ("TRANSFER_MODE",           0x00C, 0x0000),
        CMD                     ("CMD",                     0x00E, 0x0000),
        RESPONSE                ("RESPONSE",                0X010, 0X00),
        BUFFER_DATA_PORT        ("BUFFER_DATA_PORT",        0x020, 0x00000000),
        PRESENT_STATE           ("PRESENT_STATE",           0x024, CARD_INSERTED),
        HOST_CONTROL_1          ("HOST_CONTROL_1",          0x028, 0x00),
        POWER_CTRL              ("POWER_CTRL",              0x029, 0x0E),           // set Voltage to 3.3V (zero was not permitted)
        CLOCK_CTRL              ("CLOCK_CTRL",              0x02C, 0x0000),
        TIMEOUT_CTRL            ("TIMEOUT_CTRL",            0x02E, 0x00),
        SOFTWARE_RESET          ("SOFTWARE_RESET",          0x02F, 0x00),
        NORMAL_INT_STAT         ("NORMAL_INT_STAT",         0x030, 0x0000),
        ERROR_INT_STAT          ("ERROR_INT_STAT",          0x032, 0x0000),
        NORMAL_INT_STAT_ENABLE  ("NORMAL_INT_STAT_ENABLE",  0x034, 0x0000),
        ERROR_INT_STAT_ENABLE   ("ERROR_INT_STAT_ENABLE",   0x036, 0x0000),
        NORMAL_INT_SIG_ENABLE   ("NORMAL_INT_SIG_ENABLE",   0x038, 0x0000),
        ERROR_INT_SIG_ENABLE    ("ERROR_INT_SIG_ENABLE",    0x03A, 0x0000),
        CAPABILITIES            ("CAPABILITIES",            0x040, 0x00000000),     // see SD Host Controller Simplified Specification page 103
        MAX_CURR_CAP            ("MAX_CURR_CAP",            0x048, 0x00000001),     // set max current to 4 mA (zero was not permitted)
        HOST_CONTROLLER_VERSION ("HOST_CONTROLLER_VERSION", 0x0FE, 0x0000),         // set the Host  Controller Version to 1.0
        F_SDH30_AHB_CONFIG      ("F_SDH30_AHB_CONFIG",      0x100, 0x00),
        F_SDH30_ESD_CONTROL     ("F_SDH30_ESD_CONTROL",     0x124, 0x00),
        DMA_enabled("DMA_enabled", true),
        IRQ("IRQ"),
        IN("IN"),
        OUT("OUT"),
        SD_OUT("SD_OUT") {

        SDMA_SYSTEM_ADDRESS.allow_read_write();
        SDMA_SYSTEM_ADDRESS.write = &sdhci::write_SDMA_SYSTEM_ADDRESS;

        BLOCK_SIZE.allow_read_write();
        BLOCK_COUNT_16BIT.allow_read_write();
        ARG.allow_read_write();
        TRANSFER_MODE.allow_read_write();

        CMD.allow_read_write();
        CMD.read = &sdhci::read_CMD;
        CMD.write = &sdhci::write_CMD;

        RESPONSE.allow_read();

        BUFFER_DATA_PORT.allow_read_write();
        BUFFER_DATA_PORT.read = &sdhci::read_BUFFER_DATA_PORT;
        BUFFER_DATA_PORT.write = &sdhci::write_BUFFER_DATA_PORT;

        PRESENT_STATE.allow_read();
        HOST_CONTROL_1.allow_read_write();
        POWER_CTRL.allow_read_write();

        CLOCK_CTRL.allow_read_write();
        CLOCK_CTRL.read = &sdhci::read_CLOCK_CTRL;
        CLOCK_CTRL.write = &sdhci::write_CLOCK_CTRL;

        TIMEOUT_CTRL.allow_read_write();

        SOFTWARE_RESET.allow_read_write();
        SOFTWARE_RESET.read = &sdhci::read_SOFTWARE_RESET;
        SOFTWARE_RESET.write = &sdhci::write_SOFTWARE_RESET;

        NORMAL_INT_STAT.allow_read_write();
        NORMAL_INT_STAT.read = &sdhci::read_NORMAL_INT_STAT;
        NORMAL_INT_STAT.write = &sdhci::write_NORMAL_INT_STAT;

        ERROR_INT_STAT.allow_read_write();
        ERROR_INT_STAT.read = &sdhci::read_ERROR_INT_STAT;
        ERROR_INT_STAT.write = &sdhci::write_ERROR_INT_STAT;

        NORMAL_INT_STAT_ENABLE.allow_read_write();
        ERROR_INT_STAT_ENABLE.allow_read_write();
        NORMAL_INT_SIG_ENABLE.allow_read_write();
        ERROR_INT_SIG_ENABLE.allow_read_write();

        CAPABILITIES.allow_read();
        CAPABILITIES.read = &sdhci::read_CAPABILITIES;
        CAPABILITIES[0] = CAPABILITY_VALUES_0;                  // initialize the lower byte of the CAPABILITIES register; doing it in the initializer list would initialize the lower and upper byte with the same value

        MAX_CURR_CAP.allow_read();
        HOST_CONTROLLER_VERSION.allow_read();
        F_SDH30_AHB_CONFIG.allow_read_write();
        F_SDH30_ESD_CONTROL.allow_read_write();

        // for DMA
        SC_THREAD(exec_DMA_transfer);

        SD_OUT.bind(*this);
    }

    sdhci::~sdhci() {
        // nothing to do
    }
}}
