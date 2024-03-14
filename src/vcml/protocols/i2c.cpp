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

static i2c_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<i2c_base_initiator_socket*>(port);
}

static i2c_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<i2c_base_target_socket*>(port);
}

static i2c_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<i2c_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<i2c_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static i2c_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<i2c_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<i2c_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

i2c_base_initiator_socket& i2c_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

i2c_base_initiator_socket& i2c_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

i2c_base_target_socket& i2c_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

i2c_base_target_socket& i2c_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
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
    auto* tgt = dynamic_cast<i2c_target_array*>(child);
    VCML_ERROR_ON(!tgt, "%s is not a valid i2c socket array", child->name());
    tgt->get(i).set_address(a);
}

void i2c_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid i2c socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void i2c_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    i2c_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    i2c_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid i2c socket array", child->name());
}

void i2c_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid i2c port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid i2c port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void i2c_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid i2c port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid i2c port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void i2c_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid i2c port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid i2c port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void i2c_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid i2c port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid i2c port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

} // namespace vcml
