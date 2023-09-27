/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_NET_H
#define VCML_VIRTIO_NET_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/virtio.h"
#include "vcml/protocols/eth.h"

namespace vcml {
namespace virtio {

class net : public module, public virtio_device, public eth_host
{
public:
    enum virtqueues : int {
        VIRTQUEUE_RX = 0,
        VIRTQUEUE_TX = 1,
        VIRTQUEUE_CTRL,
    };

    enum features : u64 {
        VIRTIO_NET_F_MTU = bit(3),
        VIRTIO_NET_F_MAC = bit(5),
        VIRTIO_NET_F_STATUS = bit(16),
        VIRTIO_NET_F_CTRL_VQ = bit(17),
        VIRTIO_NET_F_CTRL_RX = bit(18),
        VIRTIO_NET_F_CTRL_VLAN = bit(19),
        VIRTIO_NET_F_CTRL_RX_EXTRA = bit(20),
        VIRTIO_NET_F_CTRL_ANNOUNCE = bit(21),
        VIRTIO_NET_F_CTRL_MAC_ADDR = bit(23),
    };

private:
    struct config {
        u8 mac[6];
        u16 status;
        u16 max_vq_pairs;
        u16 mtu;
    } m_config;

    mac_addr m_mac;

    bool m_promisc;
    bool m_allmulti;
    bool m_alluni;
    bool m_nomulti;
    bool m_nouni;
    bool m_nobcast;

    vector<mac_addr> m_unicast;
    vector<mac_addr> m_multicast;

    sc_event m_rxev;
    sc_event m_txev;

    bool filter(const eth_frame& frame);

    void handle_ctrl();
    void handle_ctrl_rx(vq_message& msg);
    void handle_ctrl_announce(vq_message& msg);
    void handle_ctrl_mac_addr(vq_message& msg);

    bool handle_rx(vq_message& msg, eth_frame& frame);
    bool handle_tx(vq_message& msg);

    void rx_thread();
    void tx_thread();

    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

    virtual void eth_link_up() override;
    virtual void eth_link_down() override;
    virtual void eth_receive(const eth_frame& frame) override;

public:
    property<string> mac;
    property<u16> mtu;

    virtio_target_socket virtio_in;
    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    net(const sc_module_name& nm);
    virtual ~net();
    VCML_KIND(virtio::net);

    virtual void reset();
};

} // namespace virtio
} // namespace vcml

#endif
