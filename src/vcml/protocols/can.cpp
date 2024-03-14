/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/can.h"

namespace vcml {

u8 len2dlc(size_t len) {
    if (len > 64)
        return 0xf;
    if (len <= 8)
        return len;
    if (len <= 12)
        return 9;
    if (len <= 16)
        return 10;
    if (len <= 20)
        return 11;
    if (len <= 24)
        return 12;
    if (len <= 32)
        return 13;
    if (len <= 40)
        return 14;
    return 15;
}

size_t dlc2len(u8 dlc) {
    static const size_t len[16] = { 0, 1,  2,  3,  4,  5,  6,  7,
                                    8, 12, 16, 20, 24, 32, 48, 64 };
    return len[dlc & 0xf];
}

bool can_frame::operator==(const can_frame& other) {
    return msgid == other.msgid && dlc == other.dlc && flags == other.flags &&
           memcmp(data, other.data, length()) == 0;
}

bool can_frame::operator!=(const can_frame& other) {
    return !operator==(other);
}

ostream& operator<<(ostream& os, const can_frame& frame) {
    stream_guard guard(os);
    os << "CAN";
    if (frame.is_fdf())
        os << "FD";
    if (frame.is_rtr())
        os << " +rtr";
    if (frame.is_err())
        os << " +err";
    if (frame.is_brs())
        os << " +brs";
    if (frame.is_esi())
        os << " +esi";

    os << mkstr(" %x [%02hhx]", frame.id(), frame.flags);

    size_t len = frame.length();
    if (len == 0)
        os << " <no data>";

    for (size_t i = 0; i < len; i++)
        os << mkstr(" %02hhx", frame.data[i]);

    return os;
}

void can_host::can_receive(const can_target_socket& sock, can_frame& frame) {
    can_receive(frame);
}

void can_host::can_receive(can_frame& frame) {
    m_rx_queue.push(frame);
}

bool can_host::can_rx_pop(can_frame& frame) {
    if (m_rx_queue.empty())
        return false;

    frame = m_rx_queue.front();
    m_rx_queue.pop();
    return true;
}

can_base_initiator_socket::can_base_initiator_socket(const char* nm,
                                                     address_space space):
    can_base_initiator_socket_b(nm, space), m_stub(nullptr) {
}

can_base_initiator_socket::~can_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void can_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new can_target_stub(basename());
    bind(m_stub->can_rx);
}

can_base_target_socket::can_base_target_socket(const char* nm,
                                               address_space space):
    can_base_target_socket_b(nm, space), m_stub(nullptr) {
}

can_base_target_socket::~can_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void can_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new can_initiator_stub(basename());
    m_stub->can_tx.bind(*this);
}

can_initiator_socket::can_initiator_socket(const char* nm, address_space as):
    can_base_initiator_socket(nm, as),
    m_host(hierarchy_search<can_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside can_host", name());
}

void can_initiator_socket::send(can_frame& frame) {
    trace_fw(frame);
    get_interface(0)->can_transport(frame);
    trace_bw(frame);
}

void can_target_socket::can_transport(can_frame& frame) {
    trace_fw(frame);
    m_host->can_receive(*this, frame);
    trace_bw(frame);
}

can_target_socket::can_target_socket(const char* nm, address_space as):
    can_base_target_socket(nm, as),
    m_host(hierarchy_search<can_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside can_host", name());
}

can_initiator_stub::can_initiator_stub(const char* nm):
    can_bw_transport_if(),
    can_tx(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    can_tx.bind(*this);
}

void can_target_stub::can_transport(can_frame& frame) {
    // nothing to do
}

can_target_stub::can_target_stub(const char* nm):
    can_fw_transport_if(),
    can_rx(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    can_rx.bind(*this);
}

static can_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<can_base_initiator_socket*>(port);
}

static can_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<can_base_target_socket*>(port);
}

static can_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<can_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<can_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static can_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<can_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<can_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

can_base_initiator_socket& can_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

can_base_initiator_socket& can_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

can_base_target_socket& can_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

can_base_target_socket& can_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void can_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid can socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void can_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    can_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    can_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid can socket array", child->name());
}

void can_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid can port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid can port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void can_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid can port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid can port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void can_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid can port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid can port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void can_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid can port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid can port", p2->name());

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
