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

#ifndef VCML_PROTOCOLS_SD_H
#define VCML_PROTOCOLS_SD_H

#include "vcml/common/types.h"
#include "vcml/common/systemc.h"
#include "vcml/component.h"

namespace vcml {

    enum sd_status : int {
        SD_INCOMPLETE    =  0, // command has not yet been processed
        SD_OK            =  1, // command has fully completed
        SD_OK_TX_RDY     =  2, // command done, data available for reading
        SD_OK_RX_RDY     =  3, // command done, awaiting data for writing
        SD_ERR_CRC       = -1, // command checksum error
        SD_ERR_ARG       = -2, // invalid command argument error
        SD_ERR_ILLEGAL   = -3, // illegal command error
    };

    enum sd_status_tx {
        SDTX_INCOMPLETE  =  0, // request has not yet been processed
        SDTX_OK          =  1, // next token ready
        SDTX_OK_BLK_DONE =  2, // one block fully transmitted
        SDTX_OK_COMPLETE =  3, // transmission completed
        SDTX_ERR_ILLEGAL = -1, // not transmitting
    };

    enum sd_status_rx {
        SDRX_INCOMPLETE  =  0, // request has not yet been processed
        SDRX_OK          =  1, // ready for next token
        SDRX_OK_BLK_DONE =  2, // data for one block received
        SDRX_OK_COMPLETE =  3, // data received successfully
        SDRX_ERR_CRC     = -1, // checksum error
        SDRX_ERR_INT     = -2, // internal error
        SDRX_ERR_ILLEGAL = -3, // not receiving
    };

    struct sd_command {
        u8   opcode;
        u32  argument;
        u8   crc;
        u8   response[17];
        u32  resp_len;
        bool appcmd;
        bool spi;
        sd_status status;
    };

    enum sd_mode {
        SD_READ,
        SD_WRITE,
    };

    struct sd_data {
        sd_mode mode;
        u8 data;
        union {
            sd_status_tx read;
            sd_status_rx write;
        } status;
    };

    inline bool success(sd_status    status) { return status > 0; }
    inline bool success(sd_status_tx status) { return status > 0; }
    inline bool success(sd_status_rx status) { return status > 0; }

    inline bool failed(sd_status    status) { return status < 0; }
    inline bool failed(sd_status_tx status) { return status < 0; }
    inline bool failed(sd_status_rx status) { return status < 0; }


    template <> inline bool success(const sd_command& cmd) {
        return success(cmd.status);
    }

    template <> inline bool failed(const sd_command& cmd) {
        return failed(cmd.status);
    }

    template <> inline bool success(const sd_data& data) {
        switch (data.mode) {
        case SD_READ: return success(data.status.read);
        case SD_WRITE: return success(data.status.write);
        default:
            VCML_ERROR("illegal sd mode: %d", (int)data.mode);
        }
    }

    template <> inline bool failed(const sd_data& data) {
        switch (data.mode) {
        case SD_READ: return failed(data.status.read);
        case SD_WRITE: return failed(data.status.write);
        default:
            VCML_ERROR("illegal sd mode: %d", (int)data.mode);
        }
    }


    const char* sd_status_str(sd_status status);
    const char* sd_status_str(sd_status_tx status);
    const char* sd_status_str(sd_status_rx status);
    const char* sd_opcode_str(u8 opcode, bool appcmd = false);

    ostream& operator << (ostream& os, const sd_command& tx);
    ostream& operator << (ostream& os, const sd_data& tx);

    class sd_initiator_socket;
    class sd_target_socket;
    class sd_initiator_stub;
    class sd_target_stub;

    class sd_host
    {
    public:
        sd_host() = default;
        virtual ~sd_host() = default;

        virtual void sd_transport(const sd_target_socket&, sd_command&) = 0;
        virtual void sd_transport(const sd_target_socket&, sd_data&) = 0;
    };

    class sd_fw_transport_if: public sc_core::sc_interface
    {
    public:
        virtual void sd_transport(sd_command& cmd) = 0;
        virtual void sd_transport(sd_data& data) = 0;
    };

    class sd_bw_transport_if: public sc_core::sc_interface {
        // empty interface
    };

    class sd_initiator_socket: protected sd_bw_transport_if,
        public tlm::tlm_base_initiator_socket<1, sd_fw_transport_if,
                                              sd_bw_transport_if, 1,
                                              sc_core::SC_ONE_OR_MORE_BOUND> {
    private:
        module* m_parent;
        sd_target_stub* m_stub;

    public:
        sd_initiator_socket();
        explicit sd_initiator_socket(const char* name);
        virtual ~sd_initiator_socket();

        VCML_KIND(sd_initiator_socket);
        virtual sc_core::sc_type_index get_protocol_types() const;

        void transport(sd_command& cmd);
        void transport(sd_data& data);

        sd_status_tx read_data(u8& data);
        sd_status_rx write_data(u8 data);

        void stub();
    };

    class sd_target_socket: protected sd_fw_transport_if,
        public tlm::tlm_base_target_socket<1, sd_fw_transport_if,
                                              sd_bw_transport_if, 1,
                                              sc_core::SC_ONE_OR_MORE_BOUND> {
    private:
        module* m_parent;
        sd_host* m_host;
        sd_initiator_stub* m_stub;

        virtual void sd_transport(sd_command& cmd) override;
        virtual void sd_transport(sd_data& data) override;

    public:
        sd_target_socket();
        explicit sd_target_socket(const char* name);
        virtual ~sd_target_socket();

        VCML_KIND(sd_target_socket);
        virtual sc_core::sc_type_index get_protocol_types() const;

        void stub();
    };

    class sd_initiator_stub: public module, protected sd_bw_transport_if
    {
    public:
        sd_initiator_socket SD_OUT;
        sd_initiator_stub(const sc_module_name& name);
        virtual ~sd_initiator_stub() = default;
        VCML_KIND(sd_initiator_stub);
    };

    class sd_target_stub: public module, protected sd_fw_transport_if
    {
    protected:
        virtual void sd_transport(sd_command& cmd) override;
        virtual void sd_transport(sd_data& data) override;

    public:
        sd_target_socket SD_IN;
        sd_target_stub(const sc_module_name& name);
        virtual ~sd_target_stub() = default;
        VCML_KIND(sd_target_stub);
    };

}

#endif
