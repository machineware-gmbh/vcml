/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_SD_H
#define VCML_PROTOCOLS_SD_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

enum sd_status : int {
    SD_INCOMPLETE = 0,   // command has not yet been processed
    SD_OK = 1,           // command has fully completed
    SD_OK_TX_RDY = 2,    // command done, data available for reading
    SD_OK_RX_RDY = 3,    // command done, awaiting data for writing
    SD_ERR_CRC = -1,     // command checksum error
    SD_ERR_ARG = -2,     // invalid command argument error
    SD_ERR_ILLEGAL = -3, // illegal command error
};

enum sd_status_tx {
    SDTX_INCOMPLETE = 0,   // request has not yet been processed
    SDTX_OK = 1,           // next token ready
    SDTX_OK_BLK_DONE = 2,  // one block fully transmitted
    SDTX_OK_COMPLETE = 3,  // transmission completed
    SDTX_ERR_ILLEGAL = -1, // not transmitting
};

enum sd_status_rx {
    SDRX_INCOMPLETE = 0,   // request has not yet been processed
    SDRX_OK = 1,           // ready for next token
    SDRX_OK_BLK_DONE = 2,  // data for one block received
    SDRX_OK_COMPLETE = 3,  // data received successfully
    SDRX_ERR_CRC = -1,     // checksum error
    SDRX_ERR_INT = -2,     // internal error
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

void sd_reset(sd_command& cmd);
u8 sd_crc7(const sd_command& cmd);

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

void sd_init_read(sd_data& data);
void sd_init_write(sd_data& data);

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
    sd_host() = default;
    virtual ~sd_host() = default;
    virtual void sd_transport(const sd_target_socket&, sd_command&) = 0;
    virtual void sd_transport(const sd_target_socket&, sd_data&) = 0;
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
    virtual void sd_transport(sd_data& data) = 0;
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

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

using sd_base_initiator_array = socket_array<sd_base_initiator_socket>;
using sd_base_target_array = socket_array<sd_base_target_socket>;

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
    virtual ~sd_initiator_socket() = default;
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
    virtual ~sd_target_socket() = default;
    VCML_KIND(sd_target_socket);
};

using sd_initiator_array = socket_array<sd_initiator_socket>;
using sd_target_array = socket_array<sd_target_socket>;

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

sd_base_initiator_socket& sd_initiator(const sc_object& parent,
                                       const string& port);
sd_base_initiator_socket& sd_initiator(const sc_object& parent,
                                       const string& port, size_t idx);

sd_base_target_socket& sd_target(const sc_object& parent, const string& port);
sd_base_target_socket& sd_target(const sc_object& parent, const string& port,
                                 size_t idx);

void sd_stub(const sc_object& obj, const string& port);
void sd_stub(const sc_object& obj, const string& port, size_t idx);

void sd_bind(const sc_object& obj1, const string& port1, const sc_object& obj2,
             const string& port2);
void sd_bind(const sc_object& obj1, const string& port1, const sc_object& obj2,
             const string& port2, size_t idx2);
void sd_bind(const sc_object& obj1, const string& port1, size_t idx1,
             const sc_object& obj2, const string& port2);
void sd_bind(const sc_object& obj1, const string& port1, size_t idx1,
             const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
