/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/eth.h"

#if defined(MWR_MSVC)
#define sscanf sscanf_s
#endif

namespace vcml {

const char* mac_addr::format = "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx";

mac_addr::mac_addr(const char* str): bytes() {
    int n = sscanf(str, mac_addr::format, bytes.data(), bytes.data() + 1,
                   bytes.data() + 2, bytes.data() + 3, bytes.data() + 4,
                   bytes.data() + 5);
    VCML_ERROR_ON(n != 6, "failed to parse mac address '%s'", str);
}

mac_addr::operator u64() const {
    u64 result = 0;
    for (size_t i = 0; i < bytes.size(); i++)
        result |= (u64)bytes[i] << (40 - i * 8);
    return result;
}

string mac_addr::to_string() const {
    return mkstr(format, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
                 bytes[5]);
}

eth_frame::eth_frame(const vector<u8>& raw): vector<u8>(raw) {
    if (size() > FRAME_MAX_SIZE)
        VCML_ERROR("payload too big");
    while (size() < FRAME_MIN_SIZE)
        push_back(0);
}

eth_frame::eth_frame(vector<u8>&& frame): vector<u8>(std::move(frame)) {
    if (size() > FRAME_MAX_SIZE)
        VCML_ERROR("payload too big");
    while (size() < FRAME_MIN_SIZE)
        push_back(0);
}

eth_frame::eth_frame(const u8* data, size_t len):
    vector<u8>(data, data + len) {
    if (size() > FRAME_MAX_SIZE)
        VCML_ERROR("payload too big");
    while (size() < FRAME_MIN_SIZE)
        push_back(0);
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

u16 eth_frame::ether_type() const {
    u16 type = bswap(read<u16>(12));
    if (type == ETHER_TYPE_VLAN)
        type = bswap(read<u16>(16));
    return type;
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

    case ETHER_TYPE_PTP:
        return "ETHERNET_PTP";

    case ETHER_TYPE_AVTP:
        return "ETHERNET_AVTP";

    default:
        return mkstr("ETHERNET_0x%02hx", type);
    }
}

bool eth_frame::is_nc() const {
    u64 dest = destination();
    return dest == 0x180c200000e && ether_type() == ETHER_TYPE_PTP;
}

bool eth_frame::is_avtp() const {
    u64 dest = destination();
    if (dest < 0x91e0f0000000)
        return false;
    if (dest > 0x91e0f000feff)
        return false;
    return ether_type() == ETHER_TYPE_AVTP;
}

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
    m_initiator_sockets(), m_target_sockets(), m_rx_queue(), m_link_up(true) {
    module* host = hierarchy_search<module>();
    VCML_ERROR_ON(!host, "eth_host declared without module");
    host->register_command("link_up", 0, this, &eth_host::cmd_link_up,
                           "link_up [sockets...] trigger link-up event");
    host->register_command("link_down", 0, this, &eth_host::cmd_link_down,
                           "link_up [sockets...] trigger link-down event");
    host->register_command("link_status", 0, this, &eth_host::cmd_link_status,
                           "link_status [sockets...] shows link status");
}

void eth_host::eth_receive(const eth_target_socket& sock,
                           const eth_frame& frame) {
    eth_receive(frame);
}

void eth_host::eth_receive(const eth_frame& frame) {
    m_rx_queue.push(frame);
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
    auto guard = get_hierarchy_scope();
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
    auto guard = get_hierarchy_scope();
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

static eth_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<eth_base_initiator_socket*>(port);
}

static eth_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<eth_base_target_socket*>(port);
}

static eth_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<eth_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<eth_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static eth_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<eth_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<eth_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

eth_base_initiator_socket& eth_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

eth_base_initiator_socket& eth_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

eth_base_target_socket& eth_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

eth_base_target_socket& eth_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void eth_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid ethernet socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void eth_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    eth_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    eth_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid ethernet socket array", child->name());
}

void eth_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid ethernet port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid ethernet port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void eth_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid ethernet port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid ethernet port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void eth_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid ethernet port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid ethernet port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void eth_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid ethernet port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid ethernet port", p2->name());

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
