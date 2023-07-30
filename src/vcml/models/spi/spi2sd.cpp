/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/spi2sd.h"

namespace vcml {
namespace spi {

u8 spi2sd::new_command(u8 val) {
    m_cmd.spi = true;
    m_cmd.appcmd = false;
    m_cmd.opcode = val & 0x3f;
    m_cmd.argument = 0;
    m_cmd.resp_len = 0;
    m_cmd.status = SD_INCOMPLETE;

    m_argbytes = 0;
    m_state = READ_ARGUMENT;

    return 0xff;
}

u8 spi2sd::do_spi_transport(u8 mosi) {
    switch (m_state) {
    case IDLE:
        if (mosi <= 0x7f)
            return new_command(mosi);
        return 0xff;

    case READ_ARGUMENT:
        m_cmd.argument = (m_cmd.argument << 8) | mosi;
        if (++m_argbytes == sizeof(m_cmd.argument))
            m_state = READ_CHECKSUM;
        return 0xff;

    case READ_CHECKSUM:
        m_cmd.crc = mosi;
        m_state = DO_COMMAND;
        return 0xff;

    case DO_COMMAND: {
        sd_out.transport(m_cmd);
        m_rspbytes = 0;
        m_state = DO_RESPONSE;
        return 0xff;
    }

    case DO_RESPONSE: {
        if (m_rspbytes < m_cmd.resp_len)
            return m_cmd.response[m_rspbytes++];

        switch (m_cmd.status) {
        case SD_OK_TX_RDY:
            m_state = TX_STANDBY;
            break;
        case SD_OK_RX_RDY:
            m_state = RX_STANDBY;
            break;
        default:
            m_state = IDLE;
            break;
        }

        return 0xff;
    }

    case TX_STANDBY: {
        if (mosi <= 0x7f)
            return new_command(mosi);
        m_state = TX_SENDING;
        return SPITX_GO;
    }

    case TX_SENDING: {
        if (mosi <= 0x7f)
            return new_command(mosi);

        sd_data tx;
        tx.data = 0;
        tx.mode = SD_READ;
        tx.status.read = SDTX_INCOMPLETE;
        sd_out.transport(tx);

        switch (tx.status.read) {
        case SDTX_OK:
            break;
        case SDTX_OK_BLK_DONE:
            m_state = TX_STANDBY;
            break;
        case SDTX_OK_COMPLETE:
            m_state = IDLE;
            break;
        case SDTX_ERR_ILLEGAL:
            return SPITX_ERR;
        default:
            VCML_ERROR("card returned status error");
        }

        return tx.data;
    }

    case RX_STANDBY: {
        if (mosi <= 0x7f)
            return new_command(mosi);

        switch (mosi) {
        case SPIRX_STOP:
            m_state = IDLE;
            break;
        case SPIRX_GO:
            m_state = RX_RECORDING;
            break;
        case SPITX_GO:
            m_state = RX_RECORDING;
            break;
        default:
            break;
        }

        return 0xff;
    }

    case RX_RECORDING: {
        sd_data tx;
        tx.data = mosi;
        tx.mode = SD_WRITE;
        tx.status.write = SDRX_INCOMPLETE;
        sd_out.transport(tx);

        switch (tx.status.write) {
        case SDRX_OK:
            break;
        case SDRX_OK_BLK_DONE:
            m_state = RX_STANDBY;
            return SPIRX_OK;
        case SDRX_OK_COMPLETE:
            m_state = IDLE;
            return SPIRX_OK;
        case SDRX_ERR_CRC:
            return SPIRX_ERR_CRC;
        case SDRX_ERR_INT:
            return SPIRX_ERR_WR;
        case SDRX_ERR_ILLEGAL:
            return SPIRX_ERR_WR;
        default:
            VCML_ERROR("card returned invalid write response");
        }

        return 0xff;
    }

    default:
        VCML_ERROR("invalid state %d", m_state);
    }

    return 0xff;
}

spi2sd::spi2sd(const sc_module_name& nm):
    component(nm),
    spi_host(),
    m_state(IDLE),
    m_argbytes(0),
    m_rspbytes(0),
    m_cmd(),
    cs_active_high("cs_active_high", false),
    cs("cs"),
    spi_in("spi_in"),
    sd_out("sd_out") {
}

spi2sd::~spi2sd() {
    // nothing to do
}

void spi2sd::spi_transport(const spi_target_socket& socket, spi_payload& spi) {
    if (cs == cs_active_high)
        spi.miso = do_spi_transport(spi.mosi);
}

VCML_EXPORT_MODEL(vcml::spi::spi2sd, name, args) {
    return new spi2sd(name);
}

} // namespace spi
} // namespace vcml
