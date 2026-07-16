/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/can.h"

namespace vcml {

u16 len2dlc(size_t len, bool canxl) {
    VCML_ERROR_ON(canxl && len == 0, "CAN XL cannot have 0-length payloads");

    if (canxl)
        return len - 1;

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

size_t dlc2len(u16 dlc, bool canxl) {
    if (canxl)
        return dlc + 1;

    if (dlc >= 16)
        return dlc;

    static const size_t len[16] = { 0, 1,  2,  3,  4,  5,  6,  7,
                                    8, 12, 16, 20, 24, 32, 48, 64 };
    return len[dlc & 0xf];
}

bool can_frame::operator==(const can_frame& other) const {
    if (id() != other.id() || length() != other.length() || dlc != other.dlc)
        return false;

    if (is_cancc() != other.is_cancc() || is_canfd() != other.is_canfd() ||
        is_canxl() != other.is_canxl())
        return false;

    if (is_cancc() &&
        (eff != other.eff || rtr != other.rtr || err != other.err))
        return false;

    if (is_canfd() &&
        (eff != other.eff || brs != other.brs || esi != other.esi ||
         fdf != other.fdf || rrs != other.rrs))
        return false;

    if (is_canxl() &&
        (sdt != other.sdt || af != other.af || vcid != other.vcid ||
         eff != other.eff || esi != other.esi || sec != other.sec ||
         rrs != other.rrs))
        return false;

    if (!data.empty() && memcmp(data.data(), other.data.data(), length()) != 0)
        return false;

    return true;
}

bool can_frame::operator!=(const can_frame& other) const {
    return !operator==(other);
}

ostream& operator<<(ostream& os, const can_frame& frame) {
    stream_guard guard(os);
    if (frame.is_canxl())
        os << "CANXL";
    else if (frame.is_canfd())
        os << "CANFD";
    else
        os << "CAN";

    if (frame.eff)
        os << " +eff";
    if (frame.is_cancc() && frame.rtr)
        os << " +rtr";
    if (frame.is_cancc() && frame.err)
        os << " +err";
    if (frame.is_canfd() && frame.brs)
        os << " +brs";
    if ((frame.is_canfd() || frame.is_canxl()) && frame.esi)
        os << " +esi";
    if (frame.is_canxl() && frame.sec)
        os << " +sec";
    if ((frame.is_canfd() || frame.is_canxl()) && frame.rrs)
        os << " +rrs";

    if (frame.is_canxl()) {
        os << mkstr(" %03x [%04zx] (%02x|%02x:%08x)", frame.id(),
                    frame.length(), frame.vcid, frame.sdt, frame.af);
    } else {
        os << mkstr(" %x [%zx]", frame.id(), frame.length());
    }

    size_t len = frame.length();
    if (len == 0)
        os << " <no data>";

    for (u8 data : frame.data)
        os << mkstr(" %02hhx", data);

    return os;
}

void can_host::can_receive(const can_target_socket& sock, can_frame& frame) {
    can_receive(frame);
}

void can_host::can_receive(can_frame& frame) {
    m_rx_queue.push(frame); // create a copy
}

bool can_host::can_rx_pop(can_frame& frame) {
    if (m_rx_queue.empty())
        return false;

    frame = std::move(m_rx_queue.front());
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

void can_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = can_base_initiator_socket;
    using T = can_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void can_base_initiator_socket::stub_socket(void* data) {
    stub();
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

void can_base_target_socket::bind_socket(sc_object& obj) {
    using I = can_base_initiator_socket;
    using T = can_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void can_base_target_socket::stub_socket(void* data) {
    stub();
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

void can_stub(const sc_object& obj, const string& port) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void can_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void can_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void can_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void can_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void can_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
