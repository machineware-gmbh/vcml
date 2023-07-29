/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_SPI2SD_H
#define VCML_SPI_SPI2SD_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/sd.h"

namespace vcml {
namespace spi {

class spi2sd : public component, public spi_host
{
private:
    enum token {
        SPITX_GO = 0b11111110,      // reading and single-block writing
        SPITX_ERR = 0b00001001,     // error while reading (range)
        SPIRX_GO = 0b11111100,      // initiate multi-block writing
        SPIRX_STOP = 0b11111101,    // stop multi-block writing
        SPIRX_OK = 0b00000101,      // writing completed successfully
        SPIRX_ERR_CRC = 0b00001011, // writing encountered CRC error
        SPIRX_ERR_WR = 0b00001101,  // generic error during writing
    };

    enum state {
        IDLE,          // idle and waiting for commands
        READ_ARGUMENT, // reading the four bytes command argument
        READ_CHECKSUM, // reading one byte checksum
        DO_COMMAND,    // forwarding complete SD command to card
        DO_RESPONSE,   // sending response bytes
        TX_STANDBY,    // standby for reading card
        TX_SENDING,    // reading card contents
        RX_STANDBY,    // standby for writing card
        RX_RECORDING,  // writing card contents
    };

    state m_state;
    size_t m_argbytes;
    size_t m_rspbytes;
    sd_command m_cmd;

    u8 new_command(u8 val);
    u8 do_spi_transport(u8 val);

    // disabled
    spi2sd();
    spi2sd(const spi2sd&);

public:
    spi_target_socket spi_in;
    sd_initiator_socket sd_out;

    spi2sd(const sc_module_name& name);
    virtual ~spi2sd();
    VCML_KIND(spi::spi2sd);

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override;
};

} // namespace spi
} // namespace vcml

#endif
