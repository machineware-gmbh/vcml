/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#include "vcml/protocols/i2c.h"

namespace vcml {

const char* i2c_command_str(i2c_command cmd) {
    switch (cmd) {
    case I2C_START:
        return "I2C_START";
    case I2C_DATA:
        return "I2C_DATA";
    case I2C_STOP:
        return "I2C_STOP";
    default:
        VCML_ERROR("invalid i2c_command: %d", cmd);
    }
}

const char* i2c_response_str(i2c_response resp) {
    switch (resp) {
    case I2C_INCOMPLETE:
        return "I2C_INCOMPLETE";
    case I2C_ACK:
        return "I2C_ACK";
    case I2C_NACK:
        return "I2C_NACK";
    default:
        VCML_ERROR("invalid i2c_response: %d", resp);
    }
}

ostream& operator<<(ostream& os, i2c_command cmd) {
    return os << i2c_command_str(cmd);
}

ostream& operator<<(ostream& os, i2c_response resp) {
    return os << i2c_response_str(resp);
}

ostream& operator<<(ostream& os, const i2c_payload& tx) {
    os << tx.cmd << mkstr(" [%02hhx] ", tx.data);
    return os << "(" << tx.resp << ")";
}

i2c_base_initiator_socket::i2c_base_initiator_socket(const char* nm,
                                                     address_space space):
    i2c_base_initiator_socket_b(nm, space), m_stub(nullptr) {
}

i2c_base_initiator_socket::~i2c_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void i2c_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new i2c_target_stub(basename());
    bind(m_stub->i2c_in);
}

i2c_base_target_socket::i2c_base_target_socket(const char* nm,
                                               address_space space):
    i2c_base_target_socket_b(nm, space), m_stub(nullptr) {
}

i2c_base_target_socket::~i2c_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void i2c_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new i2c_initiator_stub(basename());
    m_stub->i2c_out.bind(*this);
}

i2c_initiator_socket::i2c_initiator_socket(const char* nm, address_space as):
    i2c_base_initiator_socket(nm, as),
    m_host(hierarchy_search<i2c_host>()),
    m_transport(this) {
    bind(m_transport);
    if (m_host)
        m_host->m_initiator_sockets.insert(this);
}

i2c_initiator_socket::~i2c_initiator_socket() {
    if (m_host)
        m_host->m_initiator_sockets.erase(this);
}

i2c_response i2c_initiator_socket::start(u8 address, tlm_command cmd) {
    if (cmd == TLM_IGNORE_COMMAND) {
        cmd = address & 1u ? TLM_READ_COMMAND : TLM_WRITE_COMMAND;
        address >>= 1;
    }

    VCML_ERROR_ON(address > 127, "invalid i2c address: %hhu", address);

    i2c_payload tx;
    tx.cmd  = I2C_START;
    tx.resp = I2C_INCOMPLETE;
    tx.data = address << 1;

    if (cmd == TLM_READ_COMMAND)
        tx.data |= 1u;

    transport(tx);
    return tx.resp;
}

i2c_response i2c_initiator_socket::stop() {
    i2c_payload tx;
    tx.cmd  = I2C_STOP;
    tx.resp = I2C_INCOMPLETE;
    tx.data = 0;
    transport(tx);
    return tx.resp;
}

i2c_response i2c_initiator_socket::transport(u8& data) {
    i2c_payload tx;
    tx.cmd  = I2C_DATA;
    tx.resp = I2C_INCOMPLETE;
    tx.data = data;
    transport(tx);
    data = tx.data;
    return tx.resp;
}

void i2c_initiator_socket::transport(i2c_payload& tx) {
    trace_fw(tx);

    for (int i = 0; i < size(); i++)
        get_interface(i)->i2c_transport(tx);

    if (tx.resp == I2C_INCOMPLETE)
        tx.resp = I2C_NACK;

    trace_bw(tx);
}

void i2c_target_socket::i2c_transport(i2c_payload& tx) {
    switch (tx.cmd) {
    case I2C_START: {
        m_state = TLM_IGNORE_COMMAND;

        u8 address = tx.data >> 1;
        if (address != m_address && address != I2C_ADDR_BCAST)
            break;

        trace_fw(tx);
        m_state = tx.data & 1 ? TLM_READ_COMMAND : TLM_WRITE_COMMAND;
        tx.resp = m_host->i2c_start(*this, m_state);
        trace_bw(tx);
        break;
    }

    case I2C_STOP:
        if (m_state == TLM_IGNORE_COMMAND)
            break;

        trace_fw(tx);
        tx.resp = m_host->i2c_stop(*this);
        m_state = TLM_IGNORE_COMMAND;
        trace_bw(tx);
        break;

    case I2C_DATA:
        if (m_state == TLM_IGNORE_COMMAND)
            break;

        trace_fw(tx);
        if (m_state == TLM_READ_COMMAND)
            tx.resp = m_host->i2c_read(*this, tx.data);
        if (m_state == TLM_WRITE_COMMAND)
            tx.resp = m_host->i2c_write(*this, tx.data);
        trace_bw(tx);
        break;

    default:
        VCML_ERROR("invalid i2c_command: %d", tx.cmd);
    }
}

void i2c_target_socket::set_address(u8 address) {
    if (address == I2C_ADDR_BCAST || address > 127)
        VCML_ERROR("invalid i2c socket address: %hhu", address);
    m_address = address;
}

i2c_target_socket::i2c_target_socket(const char* nm, address_space as):
    i2c_base_target_socket(nm, as),
    m_host(hierarchy_search<i2c_host>()),
    m_address(I2C_ADDR_INVALID),
    m_state(TLM_IGNORE_COMMAND),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside i2c_host", name());
    m_host->m_target_sockets.insert(this);
}

i2c_target_socket::~i2c_target_socket() {
    if (m_host)
        m_host->m_target_sockets.erase(this);
}

i2c_initiator_stub::i2c_initiator_stub(const char* nm):
    i2c_bw_transport_if(),
    i2c_out(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    i2c_out.bind(*this);
}

void i2c_target_stub::i2c_transport(i2c_payload& tx) {
    // nothing to do
}

i2c_target_stub::i2c_target_stub(const char* nm):
    i2c_fw_transport_if(),
    i2c_in(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    i2c_in.bind(*this);
}

} // namespace vcml
