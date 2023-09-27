/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class virtio_net_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::net virtio_net;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    static constexpr u64
        EXPECTED_FEATURES = virtio::net::VIRTIO_NET_F_MTU |
                            virtio::net::VIRTIO_NET_F_MAC |
                            virtio::net::VIRTIO_NET_F_STATUS |
                            virtio::net::VIRTIO_NET_F_CTRL_VQ |
                            virtio::net::VIRTIO_NET_F_CTRL_RX |
                            virtio::net::VIRTIO_NET_F_CTRL_RX_EXTRA |
                            virtio::net::VIRTIO_NET_F_CTRL_ANNOUNCE |
                            virtio::net::VIRTIO_NET_F_CTRL_MAC_ADDR;

    virtio_net_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_net("virtio_net"),
        out("out"),
        irq("irq") {
        virtio.virtio_out.bind(virtio_net.virtio_in);

        virtio_net.eth_rx.stub();
        virtio_net.eth_tx.stub();

        bus.bind(mem.in, 0, 0xfff);
        bus.bind(virtio.in, 0x1000, 0x1fff);

        bus.bind(out);
        bus.bind(virtio.out);

        virtio.irq.bind(irq);

        clk.bind(bus.clk);
        clk.bind(mem.clk);
        clk.bind(virtio.clk);

        rst.bind(bus.rst);
        rst.bind(mem.rst);
        rst.bind(virtio.rst);

        EXPECT_STREQ(virtio_net.kind(), "vcml::virtio::net");
    }

    virtual void run_test() override {
        enum addresses : u64 {
            NET_BASE = 0x1000,
            NET_MAGIC = NET_BASE + 0x00,
            NET_VERSION = NET_BASE + 0x04,
            NET_DEVID = NET_BASE + 0x08,
            NET_DEVF = NET_BASE + 0x10,
            NET_DEVF_SEL = NET_BASE + 0x14,
            NET_DRVF = NET_BASE + 0x20,
            NET_DRVF_SEL = NET_BASE + 0x24,
            NET_VQ_SEL = NET_BASE + 0x30,
            NET_VQ_MAX = NET_BASE + 0x34,
            NET_STATUS = NET_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(NET_MAGIC, data));
        ASSERT_EQ(data, 0x74726976);

        ASSERT_OK(out.readw(NET_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(NET_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_NET);

        ASSERT_OK(out.readw(NET_STATUS, data));
        ASSERT_EQ(data, 0);

        ASSERT_OK(out.writew(NET_DEVF_SEL, 0u));
        ASSERT_OK(out.readw(NET_DEVF, data));
        ASSERT_EQ(data & 0xffffff, EXPECTED_FEATURES);
        ASSERT_OK(out.writew(NET_DRVF_SEL, 0u));
        ASSERT_OK(out.writew(NET_DRVF, data));

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(NET_STATUS, data));

        ASSERT_OK(out.readw(NET_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        // test rx queue
        data = virtio::net::VIRTQUEUE_RX;
        ASSERT_OK(out.writew(NET_VQ_SEL, data));
        ASSERT_OK(out.readw(NET_VQ_MAX, data));
        EXPECT_EQ(data, 256);

        // test tx queue
        data = virtio::net::VIRTQUEUE_TX;
        ASSERT_OK(out.writew(NET_VQ_SEL, data));
        ASSERT_OK(out.readw(NET_VQ_MAX, data));
        EXPECT_EQ(data, 256);

        // ctrl queue should exist
        data = virtio::net::VIRTQUEUE_CTRL;
        ASSERT_OK(out.writew(NET_VQ_SEL, data));
        ASSERT_OK(out.readw(NET_VQ_MAX, data));
        EXPECT_EQ(data, 64);

        // other queues should not exist
        data = virtio::net::VIRTQUEUE_CTRL + 1;
        ASSERT_OK(out.writew(NET_VQ_SEL, data));
        ASSERT_OK(out.readw(NET_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }
};

TEST(virtio, blk) {
    virtio_net_stim stim;
    sc_core::sc_start();
}
