/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/serial.h"

namespace vcml {

const char* serial_parity_str(serial_parity par) {
    switch (par) {
    case SERIAL_PARITY_NONE:
        return "n";
    case SERIAL_PARITY_ODD:
        return "o";
    case SERIAL_PARITY_EVEN:
        return "e";
    case SERIAL_PARITY_MARK:
        return "m";
    case SERIAL_PARITY_SPACE:
        return "s";
    default:
        VCML_ERROR("invalid serial parity %d", (int)par);
    }
}

const char* serial_stop_str(serial_stop stop) {
    switch (stop) {
    case SERIAL_STOP_1:
        return "1";
    case SERIAL_STOP_2:
        return "2";
    case SERIAL_STOP_1_5:
        return "1.5";
    default:
        VCML_ERROR("invalid serial stop %d", (int)stop);
    }
}

ostream& operator<<(ostream& os, serial_parity par) {
    return os << serial_parity_str(par);
}

ostream& operator<<(ostream& os, serial_stop stop) {
    return os << serial_stop_str(stop);
}

ostream& operator<<(ostream& os, const serial_payload& tx) {
    stream_guard guard(os);
    os << "SERIAL TX" << mkstr(" [%02x] ", tx.data & tx.mask) << "("
       << std::dec << tx.baud << tx.parity << tx.width << ")";
    return os;
}

bool serial_calc_parity(u8 data, serial_parity mode) {
    switch (mode) {
    case SERIAL_PARITY_ODD:
        return parity_odd(data);
    case SERIAL_PARITY_EVEN:
        return parity_even(data);
    case SERIAL_PARITY_MARK:
        return true;
    case SERIAL_PARITY_SPACE:
    case SERIAL_PARITY_NONE:
        return false;
    default:
        VCML_ERROR("invalid serial parity: %d", (int)mode);
    }
}

bool serial_test_parity(const serial_payload& tx) {
    bool parity = (tx.data >> tx.width) & 1;
    switch (tx.parity) {
    case SERIAL_PARITY_NONE:
        return true;
    case SERIAL_PARITY_ODD:
        return parity == parity_odd(tx.data & tx.mask);
    case SERIAL_PARITY_EVEN:
        return parity == parity_even(tx.data & tx.mask);
    case SERIAL_PARITY_MARK:
        return parity;
    case SERIAL_PARITY_SPACE:
        return !parity;
    default:
        VCML_ERROR("invalid serial parity: %d", (int)tx.parity);
    }
}

void serial_host::serial_receive(const serial_target_socket& socket,
                                 serial_payload& tx) {
    serial_receive(socket, tx.data & tx.mask);
}

void serial_host::serial_receive(const serial_target_socket& socket, u8 data) {
    serial_receive(data);
}

void serial_host::serial_receive(u8 data) {
    // to be overloaded
}

serial_base_initiator_socket::serial_base_initiator_socket(
    const char* nm, address_space space):
    serial_base_initiator_socket_b(nm, space), m_stub(nullptr) {
}

serial_base_initiator_socket::~serial_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void serial_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new serial_target_stub(basename());
    bind(m_stub->serial_rx);
}

serial_base_target_socket::serial_base_target_socket(const char* nm,
                                                     address_space space):
    serial_base_target_socket_b(nm, space), m_stub(nullptr) {
}

serial_base_target_socket::~serial_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void serial_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new serial_initiator_stub(basename());
    m_stub->serial_tx.bind(*this);
}

sc_time serial_initiator_socket::cycle() const {
    double symbols = 2.0;
    if (m_parity != SERIAL_PARITY_NONE)
        symbols += 1.0;

    switch (m_stop) {
    case SERIAL_STOP_1:
        symbols += 1.0;
        break;

    case SERIAL_STOP_2:
        symbols += 2.0;
        break;
    case SERIAL_STOP_1_5:
        symbols += 1.5;
        break;
    default:
        break;
    }

    return sc_time(symbols / m_baud, SC_SEC);
}

serial_initiator_socket::serial_initiator_socket(const char* nm,
                                                 address_space as):
    serial_base_initiator_socket(nm, as),
    m_baud(SERIAL_9600BD),
    m_width(SERIAL_8_BITS),
    m_parity(SERIAL_PARITY_NONE),
    m_stop(SERIAL_STOP_1),
    m_host(hierarchy_search<serial_host>()),
    m_transport(this) {
    bind(m_transport);
}

void serial_initiator_socket::send(u8 data) {
    serial_payload tx;
    tx.data = data;
    tx.mask = serial_mask(m_width);
    tx.baud = m_baud;
    tx.width = m_width;
    tx.parity = m_parity;
    tx.stop = m_stop;

    if (serial_calc_parity(data, m_parity))
        tx.data |= 1u << m_width;

    transport(tx);
}

void serial_initiator_socket::transport(serial_payload& tx) {
    trace_fw(tx);
    get_interface(0)->serial_transport(tx);
    trace_bw(tx);
}

void serial_target_socket::serial_transport(serial_payload& tx) {
    trace_fw(tx);
    m_host->serial_receive(*this, tx);
    trace_bw(tx);
}

serial_target_socket::serial_target_socket(const char* nm, address_space as):
    serial_base_target_socket(nm, as),
    m_host(hierarchy_search<serial_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside serial_host", name());
}

serial_initiator_stub::serial_initiator_stub(const char* nm):
    serial_bw_transport_if(),
    serial_tx(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    serial_tx.bind(*this);
}

void serial_target_stub::serial_transport(serial_payload& tx) {
    // nothing to do
}

serial_target_stub::serial_target_stub(const char* nm):
    serial_fw_transport_if(),
    serial_rx(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    serial_rx.bind(*this);
}

static serial_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<serial_base_initiator_socket*>(port);
}

static serial_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<serial_base_target_socket*>(port);
}

static serial_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                          size_t idx) {
    auto* base = dynamic_cast<serial_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<serial_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static serial_base_target_socket* get_target_socket(sc_object* array,
                                                    size_t idx) {
    auto* base = dynamic_cast<serial_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<serial_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

serial_base_initiator_socket& serial_initiator(const sc_object& parent,
                                               const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

serial_base_initiator_socket& serial_initiator(const sc_object& parent,
                                               const string& port,
                                               size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

serial_base_target_socket& serial_target(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

serial_base_target_socket& serial_target(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void serial_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid serial socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void serial_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    serial_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    serial_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid serial socket array", child->name());
}

void serial_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid serial port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid serial port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void serial_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid serial port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid serial port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void serial_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid serial port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid serial port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void serial_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid serial port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid serial port", p2->name());

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
