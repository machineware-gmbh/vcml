/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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

void i2c_host::i2c_transport(i2c_target_socket& socket, i2c_payload& tx) {
    if (!stl_contains(m_state, socket.address.get()))
        m_state[socket.as] = TLM_IGNORE_COMMAND;

    tlm_command& state = m_state[socket.address];
    if (state == TLM_IGNORE_COMMAND && tx.cmd != I2C_START)
        return;

    switch (tx.cmd) {
    case I2C_START: {
        state = TLM_IGNORE_COMMAND;

        u8 address = i2c_decode_address(tx.data);
        if (address != I2C_ADDR_BCAST && address != socket.address)
            return;

        socket.trace_fw(tx);
        state = i2c_decode_tlm_command(tx.data);
        tx.resp = i2c_start(socket, state);
        socket.trace_bw(tx);
        return;
    }

    case I2C_STOP: {
        socket.trace_fw(tx);
        state = TLM_IGNORE_COMMAND;
        tx.resp = i2c_stop(socket);
        socket.trace_bw(tx);
        return;
    }

    case I2C_DATA: {
        socket.trace_fw(tx);
        if (state == TLM_READ_COMMAND)
            tx.resp = i2c_read(socket, tx.data);
        if (state == TLM_WRITE_COMMAND)
            tx.resp = i2c_write(socket, tx.data);
        socket.trace_bw(tx);
        return;
    }

    default:
        VCML_ERROR("invalid i2c command: %d", (int)tx.cmd);
    }
}

i2c_response i2c_host::i2c_start(const i2c_target_socket&, tlm_command) {
    return I2C_NACK;
}

i2c_response i2c_host::i2c_stop(const i2c_target_socket&) {
    return I2C_NACK;
}

i2c_response i2c_host::i2c_read(const i2c_target_socket&, u8& data) {
    return I2C_NACK;
}

i2c_response i2c_host::i2c_write(const i2c_target_socket&, u8 data) {
    return I2C_NACK;
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
    auto guard = get_hierarchy_scope();
    m_stub = new i2c_target_stub(basename());
    bind(m_stub->i2c_in);
}

void i2c_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = i2c_base_initiator_socket_b;
    using T = i2c_base_target_socket_b;
    bind_generic<I, T>(*this, obj);
}

void i2c_base_initiator_socket::stub_socket(void* data) {
    stub();
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
    auto guard = get_hierarchy_scope();
    m_stub = new i2c_initiator_stub(basename());
    m_stub->i2c_out.bind(*this);
}

void i2c_base_target_socket::bind_socket(sc_object& obj) {
    using I = i2c_base_initiator_socket_b;
    using T = i2c_base_target_socket_b;
    bind_generic<I, T>(*this, obj);
}

void i2c_base_target_socket::stub_socket(void* data) {
    stub();
}

i2c_initiator_socket::i2c_initiator_socket(const char* nm, address_space as):
    i2c_base_initiator_socket(nm, as),
    m_host(hierarchy_search<i2c_host>()),
    m_transport(this) {
    bind(m_transport);
}

i2c_response i2c_initiator_socket::start(u8 address, tlm_command cmd) {
    if (cmd == TLM_IGNORE_COMMAND) {
        cmd = address & 1u ? TLM_READ_COMMAND : TLM_WRITE_COMMAND;
        address >>= 1;
    }

    VCML_ERROR_ON(address > 127, "invalid i2c address: %hhu", address);

    i2c_payload tx;
    tx.cmd = I2C_START;
    tx.resp = I2C_INCOMPLETE;
    tx.data = address << 1;

    if (cmd == TLM_READ_COMMAND)
        tx.data |= 1u;

    transport(tx);
    return tx.resp;
}

i2c_response i2c_initiator_socket::stop() {
    i2c_payload tx;
    tx.cmd = I2C_STOP;
    tx.resp = I2C_INCOMPLETE;
    tx.data = 0;
    transport(tx);
    return tx.resp;
}

i2c_response i2c_initiator_socket::transport(u8& data) {
    i2c_payload tx;
    tx.cmd = I2C_DATA;
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
    m_host->i2c_transport(*this, tx);
}

void i2c_target_socket::set_address(u8 addr) {
    if (addr == I2C_ADDR_BCAST || addr > 127)
        VCML_ERROR("invalid i2c socket address: %hhu", addr);
    address = addr;
}

i2c_target_socket::i2c_target_socket(const char* nm, address_space as):
    i2c_base_target_socket(nm, as),
    m_host(hierarchy_search<i2c_host>()),
    m_transport(this),
    address(this, "address", I2C_ADDR_INVALID) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside i2c_host", name());
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

void i2c_set_address(const sc_object& obj, const string& port, u8 addr) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());
    auto* tgt = dynamic_cast<i2c_target_socket*>(child);
    VCML_ERROR_ON(!tgt, "%s is not a valid i2c socket", child->name());
    tgt->set_address(addr);
}

void i2c_set_address(const sc_object& o, const string& port, size_t i, u8 a) {
    sc_object* child = find_child(o, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", o.name(), port.c_str());
    auto* aif = dynamic_cast<socket_array_if*>(child);
    VCML_ERROR_ON(!aif, "%s is not a valid i2c socket array", child->name());
    auto* tgt = dynamic_cast<i2c_target_socket*>(aif->fetch(i, false));
    VCML_ERROR_ON(!aif, "%s[%zu] is not a valid i2c socket", child->name(), i);
    tgt->set_address(a);
}

void i2c_stub(const sc_object& obj, const string& port) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void i2c_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void i2c_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void i2c_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void i2c_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void i2c_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
