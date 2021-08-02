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

#ifndef VCML_GENERIC_SPI2SD_H
#define VCML_GENERIC_SPI2SD_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/sd.h"

#include "vcml/component.h"

namespace vcml { namespace generic {

    class spi2sd: public component,
                  public spi_host {
    private:
        enum token {
            SPITX_GO      = 0b11111110, // reading and single-block writing
            SPITX_ERR     = 0b00001001, // error while reading (range)
            SPIRX_GO      = 0b11111100, // initiate multi-block writing
            SPIRX_STOP    = 0b11111101, // stop multi-block writing
            SPIRX_OK      = 0b00000101, // writing completed successfully
            SPIRX_ERR_CRC = 0b00001011, // writing encountered CRC error
            SPIRX_ERR_WR  = 0b00001101, // generic error during writing
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

        state      m_state;
        size_t     m_argbytes;
        size_t     m_rspbytes;
        sd_command m_cmd;

        u8 new_command(u8 val);
        u8 do_spi_transport(u8 val);

        // disabled
        spi2sd();
        spi2sd(const spi2sd&);

    public:
        spi_target_socket SPI_IN;
        sd_initiator_socket SD_OUT;

        spi2sd(const sc_module_name& name);
        virtual ~spi2sd();
        VCML_KIND(spi2sd);

        virtual void spi_transport(const spi_target_socket& socket,
                                   spi_payload& spi) override;
    };

}}

#endif
