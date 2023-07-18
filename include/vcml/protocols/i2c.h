/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_I2C_H
#define VCML_PROTOCOLS_I2C_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/protocols/base.h"

namespace vcml {

enum i2c_address : u8 {
    I2C_ADDR_BCAST = 0,
    I2C_ADDR_INVALID = 0xff,
};

enum i2c_command : int {
    I2C_START = 1,
    I2C_DATA = 0,
    I2C_STOP = -1,
};

enum i2c_response : int {
    I2C_INCOMPLETE = 0,
    I2C_ACK = 1,
    I2C_NACK = -1,
};

struct i2c_payload {
    i2c_command cmd;
    i2c_response resp;
    u8 data;
};

constexpr bool success(i2c_response resp) {
    return resp > 0;
}

constexpr bool failed(i2c_response resp) {
    return resp < 0;
}

constexpr bool success(const i2c_payload& tx) {
    return success(tx.resp);
}

constexpr bool failed(const i2c_payload& tx) {
    return failed(tx.resp);
}

const char* i2c_command_str(i2c_command cmd);
const char* i2c_response_str(i2c_response resp);

ostream& operator<<(ostream& os, i2c_command cmd);
ostream& operator<<(ostream& os, i2c_response resp);
ostream& operator<<(ostream& os, const i2c_payload& tx);

class i2c_initiator_socket;
class i2c_target_socket;
class i2c_initiator_stub;
class i2c_target_stub;

class i2c_host
{
public:
    i2c_host() = default;
    virtual ~i2c_host() = default;
    i2c_host(i2c_host&&) = delete;
    i2c_host(const i2c_host&) = delete;

    virtual i2c_response i2c_start(const i2c_target_socket&, tlm_command) = 0;
    virtual i2c_response i2c_stop(const i2c_target_socket&) = 0;
    virtual i2c_response i2c_read(const i2c_target_socket&, u8& data) = 0;
    virtual i2c_response i2c_write(const i2c_target_socket&, u8 data) = 0;
};

class i2c_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef i2c_payload protocol_types;

    virtual void i2c_transport(i2c_payload& tx) = 0;
};

class i2c_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef i2c_payload protocol_types;
};

typedef multi_initiator_socket<i2c_fw_transport_if, i2c_bw_transport_if>
    i2c_base_initiator_socket_b;

typedef multi_target_socket<i2c_fw_transport_if, i2c_bw_transport_if>
    i2c_base_target_socket_b;

class i2c_base_initiator_socket : public i2c_base_initiator_socket_b
{
private:
    i2c_target_stub* m_stub;

public:
    i2c_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~i2c_base_initiator_socket();
    VCML_KIND(i2c_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class i2c_base_target_socket : public i2c_base_target_socket_b
{
private:
    i2c_initiator_stub* m_stub;

public:
    i2c_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~i2c_base_target_socket();
    VCML_KIND(i2c_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

using i2c_base_initiator_array = socket_array<i2c_base_initiator_socket>;
using i2c_base_target_array = socket_array<i2c_base_target_socket>;

class i2c_initiator_socket : public i2c_base_initiator_socket
{
private:
    i2c_host* m_host;

    struct i2c_bw_transport : i2c_bw_transport_if {
        i2c_initiator_socket* socket;
        i2c_bw_transport(i2c_initiator_socket* s):
            i2c_bw_transport_if(), socket(s) {}
        virtual ~i2c_bw_transport() = default;
    } m_transport;

public:
    i2c_initiator_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~i2c_initiator_socket() = default;
    VCML_KIND(i2c_initiator_socket);

    i2c_response start(u8 address, tlm_command cmd = TLM_IGNORE_COMMAND);
    i2c_response stop();
    i2c_response transport(u8& data);

    void transport(i2c_payload& tx);
};

class i2c_target_socket : public i2c_base_target_socket
{
private:
    i2c_host* m_host;

    u8 m_address;
    tlm_command m_state;

    struct i2c_fw_transport : i2c_fw_transport_if {
        i2c_target_socket* socket;
        i2c_fw_transport(i2c_target_socket* t):
            i2c_fw_transport_if(), socket(t) {}
        virtual ~i2c_fw_transport() = default;
        virtual void i2c_transport(i2c_payload& tx) override {
            socket->i2c_transport(tx);
        }
    } m_transport;

    void i2c_transport(i2c_payload& tx);

public:
    u8 address() const { return m_address; }
    void set_address(u8 address);

    i2c_target_socket(const char* name, address_space as = VCML_AS_DEFAULT);
    virtual ~i2c_target_socket() = default;
    VCML_KIND(i2c_target_socket);
};

class i2c_initiator_stub : private i2c_bw_transport_if
{
public:
    i2c_base_initiator_socket i2c_out;
    i2c_initiator_stub(const char* nm);
    virtual ~i2c_initiator_stub() = default;
};

class i2c_target_stub : private i2c_fw_transport_if
{
private:
    virtual void i2c_transport(i2c_payload& tx) override;

public:
    i2c_base_target_socket i2c_in;
    i2c_target_stub(const char* nm);
    virtual ~i2c_target_stub() = default;
};

using i2c_initiator_array = socket_array<i2c_initiator_socket>;
using i2c_target_array = socket_array<i2c_target_socket>;

i2c_base_initiator_socket& i2c_initiator(const sc_object& parent,
                                         const string& port);
i2c_base_initiator_socket& i2c_initiator(const sc_object& parent,
                                         const string& port, size_t idx);

i2c_base_target_socket& i2c_target(const sc_object& parent,
                                   const string& port);
i2c_base_target_socket& i2c_target(const sc_object& parent, const string& port,
                                   size_t idx);

void i2c_set_address(const sc_object&, const string& port, u8 addr);
void i2c_set_address(const sc_object&, const string& port, size_t i, u8 addr);

void i2c_stub(const sc_object& obj, const string& port);
void i2c_stub(const sc_object& obj, const string& port, size_t idx);

void i2c_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void i2c_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void i2c_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void i2c_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
