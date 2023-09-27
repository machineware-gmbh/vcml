/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/net.h"

namespace vcml {
namespace virtio {

enum virtio_net_status_bits : u16 {
    VIRTIO_NET_S_LINK_UP = bit(0),
    VIRTIO_NET_S_ANNOUNCE = bit(1),
};

struct virtio_net_hdr {
    u8 flags;
    u8 gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
    u16 num_buffers;
};

enum virtio_net_hdr_flags : u8 {
    VIRTIO_NET_HDR_F_NEEDS_CSUM = bit(0),
    VIRTIO_NET_HDR_F_DATA_VALID = bit(1),
    VIRTIO_NET_HDR_F_RSC_INFO = bit(2),
};

enum virtio_net_gso_type : u16 {
    VIRTIO_NET_HDR_GSO_NONE = 0,
    VIRTIO_NET_HDR_GSO_TCPV4 = 1,
    VIRTIO_NET_HDR_GSO_UDP = 3,
    VIRTIO_NET_HDR_GSO_TCPV6 = 4,
    VIRTIO_NET_HDR_GSO_ECN = 0x80,
};

enum virtio_net_ctrl : u8 {
    VIRTIO_NET_CTRL_RX = 0,
    VIRTIO_NET_CTRL_MAC = 1,
    VIRTIO_NET_CTRL_VLAN = 2,
    VIRTIO_NET_CTRL_ANNOUNCE = 3,
    VIRTIO_NET_CTRL_MQ = 4,
    VIRTIO_NET_CTRL_GUEST_OFFLOADS = 5,
    VIRTIO_NET_CTRL_NOTF_COAL = 6,
};

enum virtio_net_ctrl_status : u8 {
    VIRTIO_NET_CTRL_OK = 0,
    VIRTIO_NET_CTRL_ERR = 1,
};

enum virtio_net_ctrl_rx : u8 {
    VIRTIO_NET_CTRL_RX_PROMISC = 0,
    VIRTIO_NET_CTRL_RX_ALLMULTI = 1,
    VIRTIO_NET_CTRL_RX_ALLUNI = 2,
    VIRTIO_NET_CTRL_RX_NOMULTI = 3,
    VIRTIO_NET_CTRL_RX_NOUNI = 4,
    VIRTIO_NET_CTRL_RX_NOBCAST = 5,
};

enum virtio_net_ctrl_announce : u8 {
    VIRTIO_NET_CTRL_ANNOUNCE_ACK = 0,
};

enum virtio_net_ctrl_mac : u8 {
    VIRTIO_NET_CTRL_MAC_TABLE_SET = 0,
    VIRTIO_NET_CTRL_MAC_SET = 1,
};

bool net::filter(const eth_frame& frame) {
    if (m_promisc)
        return true;

    if (frame.is_multicast()) {
        if (m_nomulti)
            return false;

        if (m_allmulti)
            return true;

        for (auto& mac : m_multicast)
            if (frame.destination() == mac)
                return true;
    }

    if (frame.is_unicast()) {
        if (m_nouni)
            return false;

        if (m_alluni)
            return true;

        for (auto& mac : m_unicast)
            if (frame.destination() == mac)
                return true;
    }

    if (frame.is_broadcast()) {
        if (m_nobcast)
            return false;
        return true;
    }

    if (frame.destination() == m_mac)
        return true;

    return false;
}

void net::handle_ctrl() {
    vq_message msg;
    while (virtio_in->get(VIRTQUEUE_CTRL, msg)) {
        u8 command;
        msg.copy_in(command, 0);

        switch (command) {
        case VIRTIO_NET_CTRL_RX:
            handle_ctrl_rx(msg);
            break;
        case VIRTIO_NET_CTRL_ANNOUNCE:
            handle_ctrl_announce(msg);
            break;
        case VIRTIO_NET_CTRL_MAC:
            handle_ctrl_mac_addr(msg);
            break;
        default:
            log_warn("unsupported command class: %hhu", command);
        }

        if (!virtio_in->put(VIRTQUEUE_CTRL, msg))
            log_warn("control command failed");
    }
}

void net::handle_ctrl_rx(vq_message& msg) {
    u8 mode, on;

    msg.copy_in(mode, 1);
    msg.copy_in(on, 2);

    switch (mode) {
    case VIRTIO_NET_CTRL_RX_PROMISC:
        m_promisc = on;
        break;
    case VIRTIO_NET_CTRL_RX_ALLMULTI:
        m_allmulti = on;
        break;
    case VIRTIO_NET_CTRL_RX_ALLUNI:
        m_alluni = on;
        break;
    case VIRTIO_NET_CTRL_RX_NOMULTI:
        m_nomulti = on;
        break;
    case VIRTIO_NET_CTRL_RX_NOUNI:
        m_nouni = on;
        break;
    case VIRTIO_NET_CTRL_RX_NOBCAST:
        m_nobcast = on;
        break;
    default:
        log_warn("unknown filter mode: %hhu", mode);
    }

    msg.copy_out(VIRTIO_NET_CTRL_OK);
}

void net::handle_ctrl_announce(vq_message& msg) {
    u8 subcmd;
    msg.copy_in(subcmd, 1);
    if (subcmd == VIRTIO_NET_CTRL_ANNOUNCE_ACK) {
        m_config.status &= ~VIRTIO_NET_S_ANNOUNCE;
        msg.copy_out(VIRTIO_NET_CTRL_OK);
    } else {
        log_warn("unknown announce request: %hhu", subcmd);
        msg.copy_out(VIRTIO_NET_CTRL_ERR);
    }
}

static void parse_mac_tables(vq_message& msg, vector<mac_addr>& macs,
                             size_t& offset) {
    u32 nentries;
    msg.copy_in(nentries, offset);
    offset += sizeof(nentries);

    for (u32 i = 0; i < nentries; i++) {
        mac_addr addr;
        msg.copy_in(addr.bytes.data(), addr.bytes.size(), offset);
        macs.push_back(addr);
        offset += addr.bytes.size();
    }
}

void net::handle_ctrl_mac_addr(vq_message& msg) {
    u8 subcmd;
    msg.copy_in(subcmd, 1);

    switch (subcmd) {
    case VIRTIO_NET_CTRL_MAC_SET: {
        msg.copy_in(m_config.mac, sizeof(m_config.mac), 2);
        for (size_t i = 0; i < sizeof(m_config.mac); i++)
            m_mac[i] = m_config.mac[i];
        log_debug("setting new mac %s", to_string(m_mac).c_str());
        msg.copy_out(VIRTIO_NET_CTRL_OK);
        break;
    }

    case VIRTIO_NET_CTRL_MAC_TABLE_SET: {
        m_unicast.clear();
        m_multicast.clear();
        size_t offset = 2; // skip command + subcommand bytes
        parse_mac_tables(msg, m_unicast, offset);
        parse_mac_tables(msg, m_multicast, offset);
        msg.copy_out(VIRTIO_NET_CTRL_OK);
        break;
    }

    default:
        log_warn("unknown mac control command: %hhu", subcmd);
        msg.copy_out(VIRTIO_NET_CTRL_ERR);
        break;
    }
}

bool net::handle_rx(vq_message& msg, eth_frame& frame) {
    if (msg.length_out() < frame.size() + sizeof(virtio_net_hdr)) {
        log_warn("reception buffer too small: %u", msg.length_out());
        return false;
    }

    virtio_net_hdr hdr{};
    hdr.flags = 0;
    hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr.num_buffers = 1;

    msg.copy_out(hdr);
    msg.copy_out(frame.data(), frame.size(), sizeof(hdr));
    msg.trim(frame.size() + sizeof(hdr));

    return true;
}

bool net::handle_tx(vq_message& msg) {
    virtio_net_hdr header;

    if (msg.length_in() <= sizeof(header)) {
        log_warn("transmission buffer too small: %u", msg.length_in());
        return false;
    }

    msg.copy_in(header);

    if (header.flags) {
        log_warn("unsupported packet flags: %hhx", header.flags);
        return false;
    }

    if (header.gso_type != VIRTIO_NET_HDR_GSO_NONE) {
        log_warn("unsupported packet gso type: %hu", header.gso_type);
        return false;
    }

    eth_frame frame(msg.length_in() - sizeof(header));
    msg.copy_in(frame.data(), frame.size(), sizeof(header));

    if (frame.size() < eth_frame::FRAME_MIN_SIZE)
        frame.resize(eth_frame::FRAME_MIN_SIZE);
    if (frame.size() > m_config.mtu)
        log_warn("packet exceeds MTU: %zu bytes", frame.size());

    eth_tx.send(frame);
    return true;
}

void net::rx_thread() {
    while (true) {
        vq_message msg;
        while (!virtio_in->get(VIRTQUEUE_RX, msg))
            wait(m_rxev);

        eth_frame frame;
        while (!eth_rx_pop(frame))
            wait(m_rxev);

        if (!handle_rx(msg, frame) || !virtio_in->put(VIRTQUEUE_RX, msg))
            log_warn("packet reception failed");
    }
}

void net::tx_thread() {
    while (true) {
        vq_message msg;
        while (!virtio_in->get(VIRTQUEUE_TX, msg))
            wait(m_txev);

        if (!handle_tx(msg) || !virtio_in->put(VIRTQUEUE_TX, msg))
            log_warn("packet transmission failed");
    }
}

void net::identify(virtio_device_desc& desc) {
    reset();
    desc.device_id = VIRTIO_DEVICE_NET;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.pci_class = PCI_CLASS_NETWORK_ETHERNET;
    desc.request_virtqueue(VIRTQUEUE_RX, 256);
    desc.request_virtqueue(VIRTQUEUE_TX, 256);
    desc.request_virtqueue(VIRTQUEUE_CTRL, 64);
}

bool net::notify(u32 vqid) {
    switch (vqid) {
    case VIRTQUEUE_RX:
        m_rxev.notify(SC_ZERO_TIME);
        return true;
    case VIRTQUEUE_TX:
        m_txev.notify(SC_ZERO_TIME);
        return true;
    case VIRTQUEUE_CTRL:
        handle_ctrl();
        return true;
    default:
        log_warn("invalid virtqueue notified: %u", vqid);
        return false;
    }
}

void net::read_features(u64& features) {
    features = VIRTIO_NET_F_MTU | VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS |
               VIRTIO_NET_F_CTRL_VQ | VIRTIO_NET_F_CTRL_RX |
               VIRTIO_NET_F_CTRL_RX_EXTRA | VIRTIO_NET_F_CTRL_ANNOUNCE |
               VIRTIO_NET_F_CTRL_MAC_ADDR;
}

bool net::write_features(u64 features) {
    u64 supported = 0;
    read_features(supported);
    if (features & 0xffffff & ~supported) {
        log_warn("unsupported features requested: 0x%llx", features);
        return false;
    }

    return true;
}

bool net::read_config(const range& addr, void* ptr) {
    if (addr.end >= sizeof(m_config))
        return false;

    memcpy(ptr, (u8*)&m_config + addr.start, addr.length());
    return true;
}

bool net::write_config(const range& addr, const void* ptr) {
    return false;
}

void net::eth_link_up() {
    u16 status = m_config.status & ~VIRTIO_NET_S_LINK_UP;
    if (eth_tx.link_up() && eth_rx.link_up())
        status |= VIRTIO_NET_S_LINK_UP;
    if (status != m_config.status) {
        m_config.status = status;
        virtio_in->notify();
    }
}

void net::eth_link_down() {
    u16 status = m_config.status & ~VIRTIO_NET_S_LINK_UP;
    if (status != m_config.status) {
        m_config.status = status;
        virtio_in->notify();
    }
}

void net::eth_receive(const eth_frame& frame) {
    if (filter(frame)) {
        eth_host::eth_receive(frame);
        m_rxev.notify(SC_ZERO_TIME);
    }
}

net::net(const sc_module_name& nm):
    module(nm),
    virtio_device(),
    eth_host(),
    m_config(),
    m_mac(mac_addr::temporary()),
    m_promisc(false),
    m_allmulti(false),
    m_alluni(false),
    m_nomulti(false),
    m_nouni(false),
    m_nobcast(false),
    m_unicast(),
    m_multicast(),
    m_rxev("rxev"),
    m_txev("txev"),
    mac("mac"),
    mtu("mtu", 1500),
    virtio_in("virtio_in"),
    eth_tx("eth_tx"),
    eth_rx("eth_rx") {
    if (mac.length() > 0)
        m_mac = mac_addr(mac);

    SC_HAS_PROCESS(net);

    SC_THREAD(rx_thread);
    sensitive << m_rxev;
    dont_initialize();

    SC_THREAD(tx_thread);
    sensitive << m_txev;
    dont_initialize();
}

net::~net() {
    // nothing to do
}

void net::reset() {
    m_promisc = false;
    m_allmulti = false;
    m_alluni = false;
    m_nomulti = false;
    m_nouni = false;
    m_nobcast = false;

    m_unicast.clear();
    m_multicast.clear();

    if (mac.length() > 0)
        m_mac = mac_addr(mac);

    for (size_t i = 0; i < 6; i++)
        m_config.mac[i] = m_mac[i];

    log_debug("using mac %s", to_string(m_mac).c_str());

    m_config.status = 0;
    if (eth_rx.link_up() && eth_tx.link_up())
        m_config.status |= VIRTIO_NET_S_LINK_UP;

    m_config.max_vq_pairs = 1;
    m_config.mtu = mtu;
}

VCML_EXPORT_MODEL(vcml::virtio::net, name, args) {
    return new net(name);
}

} // namespace virtio
} // namespace vcml
