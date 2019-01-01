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

#include "vcml/models/generic/spi2sd.h"

namespace vcml { namespace generic {

    u8 spi2sd::new_command(u8 val) {
        m_cmd.spi = true;
        m_cmd.opcode = val & 0x3f;
        m_cmd.argument = 0;
        m_cmd.resp_len = 0;

        m_argbytes = 0;
        m_state = READ_ARGUMENT;

        return 0xff;
    }

    u8 spi2sd::do_spi_transport(u8 val) {
        switch (m_state) {
        case IDLE:
            if (val <= 0x7f)
                return new_command(val);
            return 0xff;

        case READ_ARGUMENT:
            m_cmd.argument = (m_cmd.argument << 8) | val;
            if (++m_argbytes == sizeof(m_cmd.argument))
                m_state = READ_CHECKSUM;
            return 0xff;

        case READ_CHECKSUM:
            m_cmd.crc = val;
            m_state = DO_COMMAND;
            return 0xff;

        case DO_COMMAND:
            m_status = SD_OUT->sd_transport(m_cmd);
            m_rspbytes = 0;
            m_state = DO_RESPONSE;
            return 0xff;

        case DO_RESPONSE:
            if (m_rspbytes < m_cmd.resp_len)
                return m_cmd.response[m_rspbytes++];

            switch (m_status) {
            case SD_OK_TX_RDY: m_state = TX_STANDBY; break;
            case SD_OK_RX_RDY: m_state = RX_STANDBY; break;
            default: m_state = IDLE; break;
            }

            return 0xff;

        case TX_STANDBY:
            if (val <= 0x7f)
                return new_command(val);
            m_state = TX_SENDING;
            return SPITX_GO;

        case TX_SENDING:
            if (val <= 0x7f)
                return new_command(val);

            switch (SD_OUT->sd_data_read(val)) {
            case SDTX_OK: break;
            case SDTX_OK_BLK_DONE: m_state = TX_STANDBY; break;
            case SDTX_OK_COMPLETE: m_state = IDLE;       break;
            case SDTX_ERR_ILLEGAL: return SPITX_ERR;
            default: VCML_ERROR("card returned status error");
            }

            return val;

        case RX_STANDBY:
            if (val <= 0x7f)
                return new_command(val);

            switch (val) {
            case SPIRX_STOP: m_state = IDLE;         break;
            case SPIRX_GO:   m_state = RX_RECORDING; break;
            case SPITX_GO:   m_state = RX_RECORDING; break;
            default: break;
            }

            return 0xff;

        case RX_RECORDING:
            switch (SD_OUT->sd_data_write(val)) {
            case SDRX_OK: break;
            case SDRX_OK_BLK_DONE: m_state = RX_STANDBY; return SPIRX_OK;
            case SDRX_OK_COMPLETE: m_state = IDLE;       return SPIRX_OK;
            case SDRX_ERR_CRC:     return SPIRX_ERR_CRC;
            case SDRX_ERR_INT:     return SPIRX_ERR_WR;
            case SDRX_ERR_ILLEGAL: return SPIRX_ERR_WR;
            default: VCML_ERROR("card returned invalid write response");
            }

            return 0xff;

        default:
            VCML_ERROR("invalid state %d", m_state);
        }

        return 0xff;
    }

    spi2sd::spi2sd(const sc_module_name& nm):
        component(nm),
        spi_fw_transport_if(),
        sd_bw_transport_if(),
        m_state(IDLE),
        m_argbytes(0),
        m_rspbytes(0),
        m_cmd(),
        m_status(),
        SPI_IN("SPI_IN"),
        SD_OUT("SD_OUT") {
        SPI_IN.bind(*this);
        SD_OUT.bind(*this);
    }

    spi2sd::~spi2sd() {
        /* nothing to do */
    }

    u8 spi2sd::spi_transport(u8 val) {
        trace_in(val);
        u8 response = do_spi_transport(val);
        trace_out(response);
        return response;
    }

}}
