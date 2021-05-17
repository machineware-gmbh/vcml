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

#ifndef VCML_SD_H
#define VCML_SD_H

#include "vcml/common/types.h"
#include "vcml/common/systemc.h"
#include "vcml/component.h"

namespace vcml {

    enum sd_status {
        SD_OK,            // command has fully completed
        SD_OK_TX_RDY,     // command done, data available for reading
        SD_OK_RX_RDY,     // command done, awaiting data for writing
        SD_ERR_CRC,       // command checksum error
        SD_ERR_ARG,       // invalid command argument error
        SD_ERR_ILLEGAL,   // illegal command error
    };

    enum sd_tx_status {
        SDTX_OK,          // next token ready
        SDTX_OK_BLK_DONE, // one block fully transmitted
        SDTX_OK_COMPLETE, // transmission completed
        SDTX_ERR_ILLEGAL, // not transmitting
    };

    enum sd_rx_status {
        SDRX_OK,          // ready for next token
        SDRX_OK_BLK_DONE, // data for one block received
        SDRX_OK_COMPLETE, // data received successfully
        SDRX_ERR_CRC,     // checksum error
        SDRX_ERR_INT,     // internal error
        SDRX_ERR_ILLEGAL, // not receiving
    };

    struct sd_command {
        u8   opcode;
        u32  argument;
        u8   crc;
        u8   response[17];
        u32  resp_len;
        bool spi;
    };

    class sd_fw_transport_if: public sc_core::sc_interface
    {
    public:
        virtual sd_status sd_transport(sd_command& cmd) = 0;
        virtual sd_tx_status sd_data_read(u8& data) = 0;
        virtual sd_rx_status sd_data_write(u8 data) = 0;
    };

    class sd_bw_transport_if: public sc_core::sc_interface {
        /* empty interface */
    };

    class sd_initiator_socket:
        public tlm::tlm_base_initiator_socket<1, sd_fw_transport_if,
                                              sd_bw_transport_if, 1,
                                              sc_core::SC_ONE_OR_MORE_BOUND> {
    public:
        sd_initiator_socket();
        explicit sd_initiator_socket(const char* name);
        VCML_KIND(sd_initiator_socket);
        virtual sc_core::sc_type_index get_protocol_types() const;
    };

    class sd_target_socket:
        public tlm::tlm_base_target_socket<1, sd_fw_transport_if,
                                              sd_bw_transport_if, 1,
                                              sc_core::SC_ONE_OR_MORE_BOUND> {
    public:
        sd_target_socket();
        explicit sd_target_socket(const char* name);
        VCML_KIND(sd_target_socket);
        virtual sc_core::sc_type_index get_protocol_types() const;
    };

    class sd_initiator_stub: public module, protected sd_bw_transport_if
    {
    public:
        sd_initiator_socket SD_OUT;
        sd_initiator_stub(const sc_module_name& name);
        virtual ~sd_initiator_stub();
        VCML_KIND(sd_initiator_stub);
    };

    class sd_target_stub: public module, protected sd_fw_transport_if
    {
    protected:
        virtual sd_status sd_transport(sd_command& cmd) override;
        virtual sd_tx_status sd_data_read(u8& data) override;
        virtual sd_rx_status sd_data_write(u8 data) override;

    public:
        sd_target_socket SD_IN;
        sd_target_stub(const sc_module_name& name);
        virtual ~sd_target_stub();
        VCML_KIND(sd_target_stub);
    };

    const char* sd_cmd_str(u8 opcode, bool appcmd = false);
    string sd_cmd_str(const sd_command& tx, bool appcmd = false);

}

#endif
