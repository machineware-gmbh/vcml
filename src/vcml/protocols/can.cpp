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
    if (frame.is_rtr())
        os << "_RTR";
    if (frame.is_err())
        os << "_ERR";

    os << mkstr(" %x [%02hhx]", frame.id(), frame.flags);

    size_t len = frame.length();
    if (len == 0)
        os << " <no data>";

    for (size_t i = 0; i < len; i++)
        os << mkstr(" %02hhx", frame.data[i]);

    return os;
}

can_initiator_socket* can_host::find_can_initiator(const string& name) const {
    for (can_initiator_socket* socket : m_initiator_sockets)
        if (name == socket->basename())
            return socket;
    return nullptr;
}

can_target_socket* can_host::find_can_target(const string& name) const {
    for (can_target_socket* socket : m_target_sockets)
        if (name == socket->basename())
            return socket;
    return nullptr;
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
    hierarchy_guard guard(this);
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
    hierarchy_guard guard(this);
    m_stub = new can_initiator_stub(basename());
    m_stub->can_tx.bind(*this);
}

can_initiator_socket::can_initiator_socket(const char* nm, address_space as):
    can_base_initiator_socket(nm, as),
    m_host(hierarchy_search<can_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside can_host", name());
    m_host->m_initiator_sockets.insert(this);
}

can_initiator_socket::~can_initiator_socket() {
    if (m_host)
        m_host->m_initiator_sockets.erase(this);
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
    m_host->m_target_sockets.insert(this);
}

can_target_socket::~can_target_socket() {
    if (m_host)
        m_host->m_target_sockets.erase(this);
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

} // namespace vcml
