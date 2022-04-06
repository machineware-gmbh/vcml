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
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

enum sd_status : int {
    SD_INCOMPLETE  = 0,  // command has not yet been processed
    SD_OK          = 1,  // command has fully completed
    SD_OK_TX_RDY   = 2,  // command done, data available for reading
    SD_OK_RX_RDY   = 3,  // command done, awaiting data for writing
    SD_ERR_CRC     = -1, // command checksum error
    SD_ERR_ARG     = -2, // invalid command argument error
    SD_ERR_ILLEGAL = -3, // illegal command error
};

enum sd_status_tx {
    SDTX_INCOMPLETE  = 0,  // request has not yet been processed
    SDTX_OK          = 1,  // next token ready
    SDTX_OK_BLK_DONE = 2,  // one block fully transmitted
    SDTX_OK_COMPLETE = 3,  // transmission completed
    SDTX_ERR_ILLEGAL = -1, // not transmitting
};

enum sd_status_rx {
    SDRX_INCOMPLETE  = 0,  // request has not yet been processed
    SDRX_OK          = 1,  // ready for next token
    SDRX_OK_BLK_DONE = 2,  // data for one block received
    SDRX_OK_COMPLETE = 3,  // data received successfully
    SDRX_ERR_CRC     = -1, // checksum error
    SDRX_ERR_INT     = -2, // internal error
    SDRX_ERR_ILLEGAL = -3, // not receiving
};

struct sd_command {
    u8 opcode;
    u32 argument;
    u8 crc;
    u8 response[17];
    u32 resp_len;
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

inline bool success(sd_status status) {
    return status > 0;
}

inline bool success(sd_status_tx status) {
    return status > 0;
}

inline bool success(sd_status_rx status) {
    return status > 0;
}

inline bool failed(sd_status status) {
    return status < 0;
}

inline bool failed(sd_status_tx status) {
    return status < 0;
}

inline bool failed(sd_status_rx status) {
    return status < 0;
}

template <>
inline bool success(const sd_command& cmd) {
    return success(cmd.status);
}

template <>
inline bool failed(const sd_command& cmd) {
    return failed(cmd.status);
}

template <>
inline bool success(const sd_data& data) {
    switch (data.mode) {
    case SD_READ:
        return success(data.status.read);
    case SD_WRITE:
        return success(data.status.write);
    default:
        VCML_ERROR("illegal sd mode: %d", (int)data.mode);
    }
}

template <>
inline bool failed(const sd_data& data) {
    switch (data.mode) {
    case SD_READ:
        return failed(data.status.read);
    case SD_WRITE:
        return failed(data.status.write);
    default:
        VCML_ERROR("illegal sd mode: %d", (int)data.mode);
    }
}

const char* sd_status_str(sd_status status);
const char* sd_status_str(sd_status_tx status);
const char* sd_status_str(sd_status_rx status);
const char* sd_opcode_str(u8 opcode, bool appcmd = false);

ostream& operator<<(ostream& os, const sd_command& tx);
ostream& operator<<(ostream& os, const sd_data& tx);

class sd_initiator_socket;
class sd_target_socket;
class sd_initiator_stub;
class sd_target_stub;

class sd_host
{
public:
    friend class sd_initiator_socket;
    friend class sd_target_socket;

    typedef vector<sd_initiator_socket*> sd_initiator_sockets;
    typedef vector<sd_target_socket*> sd_target_sockets;

    const sd_initiator_sockets& all_sd_initiator_sockets() const {
        return m_initiator_sockets;
    }

    const sd_target_sockets& all_sd_target_sockets() const {
        return m_target_sockets;
    }

    sd_target_sockets all_sd_target_sockets(address_space);

    sd_host()          = default;
    virtual ~sd_host() = default;

    virtual void sd_transport(const sd_target_socket&, sd_command&) = 0;
    virtual void sd_transport(const sd_target_socket&, sd_data&)    = 0;

private:
    sd_initiator_sockets m_initiator_sockets;
    sd_target_sockets m_target_sockets;
};

struct sd_protocol_types {
    typedef sd_command command_payload;
    typedef sd_data data_payload;
};

class sd_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef sd_protocol_types protocol_types;

    virtual void sd_transport(sd_command& cmd) = 0;
    virtual void sd_transport(sd_data& data)   = 0;
};

class sd_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef sd_protocol_types protocol_types;
};

typedef base_initiator_socket<sd_fw_transport_if, sd_bw_transport_if>
    sd_base_initiator_socket_b;

typedef base_target_socket<sd_fw_transport_if, sd_bw_transport_if>
    sd_base_target_socket_b;

class sd_base_initiator_socket : public sd_base_initiator_socket_b
{
private:
    sd_target_stub* m_stub;

public:
    sd_base_initiator_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~sd_base_initiator_socket();
    VCML_KIND(sd_base_initiator_socket);

    virtual sc_type_index get_protocol_types() const override {
        return typeid(sd_bw_transport_if::protocol_types);
    }

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class sd_base_target_socket : public sd_base_target_socket_b
{
private:
    sd_initiator_stub* m_stub;

public:
    sd_base_target_socket(const char* nm, address_space = VCML_AS_DEFAULT);
    virtual ~sd_base_target_socket();
    VCML_KIND(sd_base_target_socket);

    virtual sc_type_index get_protocol_types() const override {
        return typeid(sd_fw_transport_if::protocol_types);
    }

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

template <const size_t MAX_PORTS = SIZE_MAX>
using sd_base_initiator_socket_array = socket_array<sd_base_initiator_socket,
                                                    MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using sd_base_target_socket_array = socket_array<sd_base_target_socket,
                                                 MAX_PORTS>;

class sd_initiator_socket : public sd_base_initiator_socket
{
private:
    sd_host* m_host;

    struct sd_bw_transport : sd_bw_transport_if {
        sd_initiator_socket* socket;
        sd_bw_transport(sd_initiator_socket* parent): socket(parent) {}
        virtual ~sd_bw_transport() = default;
    } m_transport;

public:
    sd_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~sd_initiator_socket();
    VCML_KIND(sd_initiator_socket);

    void transport(sd_command& cmd);
    void transport(sd_data& data);

    sd_status_tx read_data(u8& data);
    sd_status_rx write_data(u8 data);
};

class sd_target_socket : public sd_base_target_socket
{
private:
    sd_host* m_host;

    struct sd_fw_transport : sd_fw_transport_if {
        sd_target_socket* socket;
        sd_fw_transport(sd_target_socket* parent): socket(parent) {}
        virtual ~sd_fw_transport() = default;
        virtual void sd_transport(sd_command& cmd) override {
            socket->sd_transport(cmd);
        }

        virtual void sd_transport(sd_data& data) override {
            socket->sd_transport(data);
        }
    } m_transport;

    void sd_transport(sd_command& cmd);
    void sd_transport(sd_data& data);

public:
    sd_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~sd_target_socket();
    VCML_KIND(sd_target_socket);
};

template <const size_t MAX_PORTS = SIZE_MAX>
using sd_initiator_socket_array = socket_array<sd_initiator_socket, MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using sd_target_socket_array = socket_array<sd_target_socket, MAX_PORTS>;

class sd_initiator_stub : private sd_bw_transport_if
{
public:
    sd_base_initiator_socket sd_out;
    sd_initiator_stub(const char* name);
    virtual ~sd_initiator_stub() = default;
};

class sd_target_stub : private sd_fw_transport_if
{
private:
    virtual void sd_transport(sd_command& cmd) override;
    virtual void sd_transport(sd_data& data) override;

public:
    sd_base_target_socket sd_in;
    sd_target_stub(const char* name);
    virtual ~sd_target_stub() = default;
};

} // namespace vcml

#endif
