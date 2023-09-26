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
private:
    enum virtqueues : int {
        VIRTQUEUE_RX = 0,
        VIRTQUEUE_TX = 1,
        VIRTQUEUE_CTRL,
    };

    enum features : u64 {
        VIRTIO_NET_F_MTU = bit(3),
        VIRTIO_NET_F_MAC = bit(5),
    };

    struct config {
        u8 mac[6];
        u16 status;
        u16 max_vq_pairs;
        u16 mtu;
    } m_config;

    mac_addr m_mac;

    sc_event m_rxev;
    sc_event m_txev;

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
