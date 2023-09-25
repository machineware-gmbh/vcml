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

enum virtio_netgso_type : u16 {
    VIRTIO_NET_HDR_GSO_NONE = 0,
    VIRTIO_NET_HDR_GSO_TCPV4 = 1,
    VIRTIO_NET_HDR_GSO_UDP = 3,
    VIRTIO_NET_HDR_GSO_TCPV6 = 4,
    VIRTIO_NET_HDR_GSO_ECN = 0x80,
};

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
}

bool net::notify(u32 vqid) {
    if (is_rx(vqid))
        m_rxev.notify(SC_ZERO_TIME);
    else
        m_txev.notify(SC_ZERO_TIME);
    return true;
}

void net::read_features(u64& features) {
    features = VIRTIO_NET_F_MAC;
}

bool net::write_features(u64 features) {
    if (features & ~VIRTIO_NET_F_MAC & 0xffffff) {
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
    if (frame.is_broadcast() || frame.destination() == m_mac) {
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
    m_rxev("rxev"),
    m_txev("txev"),
    mac("mac"),
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
    log_debug("using mac %s", to_string(m_mac).c_str());

    for (size_t i = 0; i < 6; i++)
        m_config.mac[i] = m_mac[i];

    m_config.status = 0;
    if (eth_rx.link_up() && eth_tx.link_up())
        m_config.status |= VIRTIO_NET_S_LINK_UP;

    m_config.max_vq_pairs = 1;
    m_config.mtu = 0;
}

VCML_EXPORT_MODEL(vcml::virtio::net, name, args) {
    return new net(name);
}

} // namespace virtio
} // namespace vcml
