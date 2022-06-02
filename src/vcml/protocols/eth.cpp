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

#include "vcml/protocols/eth.h"

namespace vcml {

const char* mac_addr::format = "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx";

mac_addr::mac_addr(const char* str): bytes() {
    int n = sscanf(str, mac_addr::format, bytes.data(), bytes.data() + 1,
                   bytes.data() + 2, bytes.data() + 3, bytes.data() + 4,
                   bytes.data() + 5);
    VCML_ERROR_ON(n != 6, "failed to parse mac address '%s'", str);
}

string mac_addr::to_string() const {
    return mkstr(format, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
                 bytes[5]);
}

eth_frame::eth_frame(const mac_addr& dest, const mac_addr& src,
                     const vector<u8>& payload) {
    insert(end(), dest.bytes.begin(), dest.bytes.end());
    insert(end(), src.bytes.begin(), src.bytes.end());

    u16 len = payload.size();
    push_back(len >> 0);
    push_back(len >> 8);

    insert(end(), payload.begin(), payload.end());

    if (size() > FRAME_MAX_SIZE)
        VCML_ERROR("payload too big");
    while (size() < FRAME_MIN_SIZE)
        push_back(0);
}

string eth_frame::identify() const {
    if (empty())
        return "ETHERNET_EMPTY";
    if (!valid())
        return "ETHERNET_INVALID";

    u16 type = ether_type();
    switch (type) {
    case ETHER_TYPE_ARP: {
        if (payload_size() > 7 && payload(7) == 1)
            return "ETHERNET_ARP_REQUEST";
        if (payload_size() > 7 && payload(7) == 2)
            return "ETHERNET_ARP_RESPONSE";
        return "ETHERNET_ARP";
    }

    case ETHER_TYPE_IPV4: {
        if (payload_size() > 9 && payload(9) == IP_TCP)
            return "ETHERNET_TCP/IP";
        if (payload_size() > 9 && payload(9) == IP_UDP)
            return "ETHERNET_UDP/IP";
        if (payload_size() > 9 && payload(9) == IP_ICMP)
            return "ETHERNET_ICMP";
        return "ETHERNET_IPv4";
    }

    case ETHER_TYPE_IPV6: {
        if (payload_size() > 6 && payload(6) == IP_TCP)
            return "ETHERNET_TCP/IPv6";
        if (payload_size() > 6 && payload(6) == IP_UDP)
            return "ETHERNET_UDP/IPv6";
        if (payload_size() > 6 && payload(6) == IP_ICMP6)
            return "ETHERNET_ICMP6";
        return "ETHERNET_IPv6";
    }

    default:
        return mkstr("ETHERNET_0x%02hx", type);
    }
} // namespace vcml

bool eth_frame::print_payload = true;
size_t eth_frame::print_payload_columns = 16;

ostream& operator<<(ostream& os, const mac_addr& addr) {
    stream_guard guard(os);
    os << addr.to_string();
    return os;
}

ostream& operator<<(ostream& os, const eth_frame& frame) {
    stream_guard guard(os);

    os << frame.identify() << " " << frame.size() << "bytes";

    if (!frame.valid())
        return os;

    os << " from " << frame.source() << " to " << frame.destination();

    if (eth_frame::print_payload && eth_frame::print_payload_columns > 0) {
        for (size_t i = 0; i < frame.size(); i++) {
            os << (i % eth_frame::print_payload_columns ? " " : "\n\t")
               << std::hex << std::setw(2) << std::setfill('0')
               << (int)frame[i] << std::dec;
        }

        os << std::endl;
    }

    return os;
}

eth_initiator_socket* eth_host::eth_find_initiator(const string& name) const {
    for (eth_initiator_socket* socket : m_initiator_sockets)
        if (name == socket->basename())
            return socket;
    return nullptr;
}

eth_target_socket* eth_host::eth_find_target(const string& name) const {
    for (eth_target_socket* socket : m_target_sockets)
        if (name == socket->basename())
            return socket;
    return nullptr;
}

eth_host::eth_host():
    m_initiator_sockets(), m_target_sockets(), m_link_up(true) {
    module* host = hierarchy_search<module>();
    VCML_ERROR_ON(!host, "eth_host declared without module");
    host->register_command("link_up", 0, this, &eth_host::cmd_link_up,
                           "link_up [sockets...] trigger link-up event");
    host->register_command("link_down", 0, this, &eth_host::cmd_link_down,
                           "link_up [sockets...] trigger link-down event");
    host->register_command("link_status", 0, this, &eth_host::cmd_link_status,
                           "link_status [sockets...] shows link status");
}

void eth_host::eth_receive(const eth_target_socket& sock, eth_frame& frame) {
    eth_receive(frame);
}

void eth_host::eth_receive(eth_frame& frame) {
    m_rx_queue.push(std::move(frame));
}

bool eth_host::eth_rx_pop(eth_frame& frame) {
    if (m_rx_queue.empty())
        return false;

    frame = std::move(m_rx_queue.front());
    m_rx_queue.pop();
    return true;
}

void eth_host::eth_link_up() {
    // to be overloaded
}

void eth_host::eth_link_up(const eth_initiator_socket& sock) {
    if (!m_link_up) {
        m_link_up = true;
        eth_link_up();
    }
}

void eth_host::eth_link_up(const eth_target_socket& sock) {
    if (!m_link_up) {
        m_link_up = true;
        eth_link_up();
    }
}

void eth_host::eth_link_down() {
    // to be overloaded
}

void eth_host::eth_link_down(const eth_initiator_socket& sock) {
    if (m_link_up) {
        m_link_up = false;
        eth_link_down();
    }
}

void eth_host::eth_link_down(const eth_target_socket& sock) {
    if (m_link_up) {
        m_link_up = false;
        eth_link_down();
    }
}

vector<string> eth_host::gather_sockets(const vector<string>& names,
                                        eth_initiator_sockets& initiators,
                                        eth_target_sockets& targets) const {
    vector<string> notfound;
    if (names.empty()) {
        initiators = m_initiator_sockets;
        targets = m_target_sockets;
    }

    for (const string& name : names) {
        eth_initiator_socket* initiator = eth_find_initiator(name);
        if (initiator != nullptr) {
            initiators.insert(initiator);
            continue;
        }

        eth_target_socket* target = eth_find_target(name);
        if (target != nullptr) {
            targets.insert(target);
            continue;
        }

        notfound.push_back(name);
    }

    return notfound;
}

bool eth_host::cmd_link_up(const vector<string>& args, ostream& os) {
    eth_initiator_sockets initiators;
    eth_target_sockets targets;
    auto notfound = gather_sockets(args, initiators, targets);
    if (!notfound.empty()) {
        os << "sockets not found:";
        for (const string& name : notfound)
            os << " " << name;
        return false;
    }

    for (eth_initiator_socket* socket : initiators)
        socket->set_link_up(true);
    for (eth_target_socket* socket : targets)
        socket->set_link_up(true);

    return true;
}

bool eth_host::cmd_link_down(const vector<string>& args, ostream& os) {
    eth_initiator_sockets initiators;
    eth_target_sockets targets;
    auto notfound = gather_sockets(args, initiators, targets);
    if (!notfound.empty()) {
        os << "sockets not found:";
        for (const string& name : notfound)
            os << " " << name;
        return false;
    }

    for (eth_initiator_socket* socket : initiators)
        socket->set_link_up(false);
    for (eth_target_socket* socket : targets)
        socket->set_link_up(false);

    return true;
}

bool eth_host::cmd_link_status(const vector<string>& args, ostream& os) {
    eth_initiator_sockets initiators;
    eth_target_sockets targets;
    gather_sockets(args, initiators, targets);

    for (eth_initiator_socket* socket : initiators)
        os << socket->basename() << " link "
           << (socket->link_up() ? "up" : "down") << std::endl;
    for (eth_target_socket* socket : targets)
        os << socket->basename() << " link "
           << (socket->link_up() ? "up" : "down") << std::endl;

    return true;
}

eth_base_initiator_socket::eth_base_initiator_socket(const char* nm,
                                                     address_space space):
    eth_base_initiator_socket_b(nm, space), m_stub(nullptr) {
}

eth_base_initiator_socket::~eth_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void eth_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new eth_target_stub(basename());
    bind(m_stub->eth_rx);
}

eth_base_target_socket::eth_base_target_socket(const char* nm,
                                               address_space space):
    eth_base_target_socket_b(nm, space), m_stub(nullptr) {
}

eth_base_target_socket::~eth_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void eth_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new eth_initiator_stub(basename());
    m_stub->eth_tx.bind(*this);
}

void eth_initiator_socket::set_link_up(bool up) {
    if (m_link_up == up)
        return;

    m_link_up = up;

    if (m_link_up)
        m_host->eth_link_up(*this);
    else
        m_host->eth_link_down(*this);
}

eth_initiator_socket::eth_initiator_socket(const char* nm, address_space as):
    eth_base_initiator_socket(nm, as),
    m_link_up(true),
    m_host(hierarchy_search<eth_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside eth_host", name());
    m_host->m_initiator_sockets.insert(this);
}

eth_initiator_socket::~eth_initiator_socket() {
    if (m_host)
        m_host->m_initiator_sockets.erase(this);
}

void eth_initiator_socket::send(const vector<u8>& data) {
    eth_frame frame(data);
    send(frame);
}

void eth_initiator_socket::send(eth_frame& frame) {
    trace_fw(frame);
    if (m_link_up)
        get_interface(0)->eth_transport(frame);
    trace_bw(frame);
}

void eth_target_socket::eth_transport(eth_frame& frame) {
    trace_fw(frame);
    if (m_link_up)
        m_host->eth_receive(*this, frame);
    trace_bw(frame);
}

void eth_target_socket::set_link_up(bool up) {
    if (m_link_up == up)
        return;

    m_link_up = up;

    if (m_link_up)
        m_host->eth_link_up(*this);
    else
        m_host->eth_link_down(*this);
}

eth_target_socket::eth_target_socket(const char* nm, address_space as):
    eth_base_target_socket(nm, as),
    m_link_up(true),
    m_host(hierarchy_search<eth_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside eth_host", name());
    m_host->m_target_sockets.insert(this);
}

eth_target_socket::~eth_target_socket() {
    if (m_host)
        m_host->m_target_sockets.erase(this);
}

eth_initiator_stub::eth_initiator_stub(const char* nm):
    eth_bw_transport_if(),
    eth_tx(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    eth_tx.bind(*this);
}

void eth_target_stub::eth_transport(eth_frame& frame) {
    // nothing to do
}

eth_target_stub::eth_target_stub(const char* nm):
    eth_fw_transport_if(),
    eth_rx(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    eth_rx.bind(*this);
}

} // namespace vcml
