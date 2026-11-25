/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "virtio_test.h"

class virtio_net_stim : public test_base, public eth_host
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::net virtio_net;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_stim stim;
    virtio::net net;
    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;
    vector<eth_frame> sent;

    const mac_addr mac;
    const mac_addr peer;
    const mac_addr other;
    const mac_addr multicast;
    const mac_addr broadcast;

    virtual void eth_receive(const eth_target_socket& socket,
                             const eth_frame& frame) override {
        sent.push_back(frame);
    }

    enum : u32 {
        VQ_RX = 0,
        VQ_TX = 1,
        VQ_CTRL = 2,
        HDR_SIZE = 12,
        RX_SIZE = 1526,
    };

    enum : u8 {
        CTRL_RX = 0,
        CTRL_MAC = 1,
        CTRL_VLAN = 2,
        CTRL_ANNOUNCE = 3,
        CTRL_RX_PROMISC = 0,
        CTRL_RX_ALLMULTI = 1,
        CTRL_RX_ALLUNI = 2,
        CTRL_RX_NOMULTI = 3,
        CTRL_RX_NOUNI = 4,
        CTRL_RX_NOBCAST = 5,
        CTRL_MAC_TABLE_SET = 0,
        CTRL_MAC_SET = 1,
        CTRL_OK = 0,
        CTRL_ERR = 1,
        CTRL_NONE = 0xff,
    };

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
        irq("irq"),
        stim("stim"),
        net("net"),
        eth_tx("eth_tx"),
        eth_rx("eth_rx"),
        sent(),
        mac("12:34:56:78:9a:bc"),
        peer("aa:bb:cc:dd:ee:ff"),
        other("12:34:56:00:00:01"),
        multicast("01:00:5e:00:00:01"),
        broadcast("ff:ff:ff:ff:ff:ff") {
        stim.virtio_out.bind(net.virtio_in);
        net.eth_tx.bind(eth_rx);
        eth_tx.bind(net.eth_rx);

        add_test("strings", &virtio_net_stim::test_strings);
        add_test("mmio", &virtio_net_stim::test_mmio);
        add_test("identify", &virtio_net_stim::test_identify);
        add_test("config", &virtio_net_stim::test_config);
        add_test("transmit", &virtio_net_stim::test_transmit);
        add_test("transmit_errors", &virtio_net_stim::test_transmit_errors);
        add_test("receive", &virtio_net_stim::test_receive);
        add_test("receive_errors", &virtio_net_stim::test_receive_errors);
        add_test("filter", &virtio_net_stim::test_filter);
        add_test("mac_address", &virtio_net_stim::test_mac_address);
        add_test("mac_table", &virtio_net_stim::test_mac_table);
        add_test("control", &virtio_net_stim::test_control);
        add_test("link", &virtio_net_stim::test_link);

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
    }

    void test_strings() {
        EXPECT_STREQ(virtio_net.kind(), "vcml::virtio::net");
        EXPECT_STREQ(virtio_net.version(), VCML_VERSION_STRING);
    }

    void test_mmio() {
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

    void settle() { wait(sc_time(1, SC_US)); }

    void prepare() {
        settle();
        stim.clear();
        stim.identify();
        sent.clear();
    }

    eth_frame make_frame(const mac_addr& dest, size_t len, u8 seed) {
        vector<u8> payload(len);
        for (size_t i = 0; i < len; i++)
            payload[i] = (u8)(seed + i);
        return eth_frame(dest, peer, payload);
    }

    void transmit(const vector<vector<u8>>& in) {
        stim.submit(VQ_TX, in, {});
        EXPECT_TRUE(stim.kick(VQ_TX));
        settle();
    }

    bool receive(const eth_frame& frame) {
        size_t done = stim.used[VQ_RX].size();
        vq_message msg = stim.submit(VQ_RX, {}, { RX_SIZE });
        EXPECT_TRUE(stim.kick(VQ_RX));
        eth_frame tx(frame);
        eth_tx.send(tx);
        settle();

        if (stim.used[VQ_RX].size() == done) {
            EXPECT_EQ(stim.avail[VQ_RX].size(), 1);
            stim.avail[VQ_RX].clear();
            return false;
        }

        EXPECT_EQ(stim.used[VQ_RX].size(), done + 1);
        const vq_message& resp = stim.used[VQ_RX].back();
        EXPECT_EQ(resp.index, msg.index);
        EXPECT_EQ(resp.length_out(), HDR_SIZE + frame.size());

        vector<u8> data = stim.response(resp);
        vector<u8> hdr(HDR_SIZE, 0);
        hdr[10] = 1;
        EXPECT_EQ(vector<u8>(data.begin(), data.begin() + HDR_SIZE), hdr);
        EXPECT_EQ(eth_frame(data.data() + HDR_SIZE, data.size() - HDR_SIZE),
                  frame);
        return true;
    }

    u8 control(const vector<u8>& command) {
        size_t done = stim.used[VQ_CTRL].size();
        vq_message msg = stim.submit(VQ_CTRL, { command }, { 1 });
        EXPECT_TRUE(stim.kick(VQ_CTRL));
        EXPECT_EQ(stim.used[VQ_CTRL].size(), done + 1);
        return stim.response(msg)[0];
    }

    void test_identify() {
        virtio_device_desc desc = stim.identify();
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_NET);
        EXPECT_EQ(desc.vendor_id, VIRTIO_VENDOR_VCML);
        EXPECT_EQ(desc.pci_class, PCI_CLASS_NETWORK_ETHERNET);
        ASSERT_EQ(desc.virtqueues.size(), 3);
        EXPECT_EQ(desc.virtqueues.at(VQ_RX).limit, 256);
        EXPECT_EQ(desc.virtqueues.at(VQ_TX).limit, 256);
        EXPECT_EQ(desc.virtqueues.at(VQ_CTRL).limit, 64);

        EXPECT_EQ(stim.features(), EXPECTED_FEATURES);
        EXPECT_TRUE(stim.virtio_out->write_features(EXPECTED_FEATURES));
        EXPECT_TRUE(stim.virtio_out->write_features(0));
        EXPECT_TRUE(stim.virtio_out->write_features(VIRTIO_F_VERSION_1));
        EXPECT_FALSE(stim.virtio_out->write_features(bit(0)));
        EXPECT_FALSE(stim.kick(5));
    }

    void test_config() {
        stim.identify();
        u8 addr[6] = {};
        u16 status = 0;
        u16 pairs = 0;
        u16 mtu = 0;
        EXPECT_TRUE(stim.read_config(0, addr));
        EXPECT_TRUE(stim.read_config(6, status));
        EXPECT_TRUE(stim.read_config(8, pairs));
        EXPECT_TRUE(stim.read_config(10, mtu));
        EXPECT_EQ(
            mac_addr(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]),
            mac);
        EXPECT_EQ(status, 1);
        EXPECT_EQ(pairs, 1);
        EXPECT_EQ(mtu, 1500);

        EXPECT_FALSE(stim.read_config(12, status));
        EXPECT_FALSE(stim.read_config(11, status));
        EXPECT_FALSE(stim.write_config(10, mtu));
    }

    void test_transmit() {
        prepare();
        eth_frame frame0 = make_frame(peer, 100, 0x10);
        eth_frame frame1 = make_frame(peer, 1500, 0x20);
        eth_frame frame2 = make_frame(peer, 10, 0x30);
        vector<u8> hdr(HDR_SIZE, 0);

        transmit({ stim.concat({ hdr, frame0 }) });
        transmit({ hdr, frame1 });
        ASSERT_EQ(sent.size(), 2);
        EXPECT_EQ(sent[0], frame0);
        EXPECT_EQ(sent[1], frame1);
        EXPECT_EQ(stim.used[VQ_TX].size(), 2);

        transmit({ hdr, frame2 });
        ASSERT_EQ(sent.size(), 3);
        eth_frame padded(frame2);
        padded.resize(eth_frame::FRAME_MIN_SIZE);
        EXPECT_EQ(sent[2], padded);
        EXPECT_EQ(stim.used[VQ_TX].size(), 3);

        stim.submit(VQ_TX, { hdr, frame0 }, {});
        stim.submit(VQ_TX, { hdr, frame1 }, {});
        EXPECT_TRUE(stim.kick(VQ_TX));
        settle();
        ASSERT_EQ(sent.size(), 5);
        EXPECT_EQ(sent[3], frame0);
        EXPECT_EQ(sent[4], frame1);
        EXPECT_EQ(stim.used[VQ_TX].size(), 5);
        EXPECT_TRUE(stim.used[VQ_RX].empty());
    }

    void test_transmit_errors() {
        prepare();
        eth_frame frame = make_frame(peer, 100, 0x40);
        vector<u8> hdr(HDR_SIZE, 0);

        transmit({ hdr });
        EXPECT_EQ(stim.used[VQ_TX].size(), 1);

        vector<u8> flags(hdr);
        flags[0] = 1;
        transmit({ flags, frame });
        EXPECT_EQ(stim.used[VQ_TX].size(), 2);

        vector<u8> gso(hdr);
        gso[1] = 1;
        transmit({ gso, frame });
        EXPECT_EQ(stim.used[VQ_TX].size(), 3);
        EXPECT_TRUE(sent.empty());

        transmit({ hdr, frame });
        EXPECT_EQ(stim.used[VQ_TX].size(), 4);
        EXPECT_EQ(sent.size(), 1);
    }

    void test_receive() {
        prepare();
        EXPECT_TRUE(receive(make_frame(mac, 100, 0x50)));
        EXPECT_TRUE(receive(make_frame(mac, 1500, 0x60)));
        EXPECT_TRUE(receive(make_frame(mac, 1, 0x70)));

        eth_frame frame0 = make_frame(mac, 64, 0x80);
        eth_frame frame1 = make_frame(mac, 64, 0x90);
        eth_frame tx(frame0);
        eth_tx.send(tx);
        tx = frame1;
        eth_tx.send(tx);
        settle();
        EXPECT_EQ(stim.used[VQ_RX].size(), 3);

        stim.submit(VQ_RX, {}, { HDR_SIZE, 100 });
        stim.submit(VQ_RX, {}, { RX_SIZE });
        EXPECT_TRUE(stim.kick(VQ_RX));
        settle();
        ASSERT_EQ(stim.used[VQ_RX].size(), 5);
        vector<u8> data = stim.response(stim.used[VQ_RX][3]);
        EXPECT_EQ(eth_frame(data.data() + HDR_SIZE, data.size() - HDR_SIZE),
                  frame0);
        data = stim.response(stim.used[VQ_RX][4]);
        EXPECT_EQ(eth_frame(data.data() + HDR_SIZE, data.size() - HDR_SIZE),
                  frame1);
        EXPECT_TRUE(sent.empty());
    }

    void test_receive_errors() {
        prepare();
        eth_frame frame = make_frame(mac, 100, 0xa0);
        stim.submit(VQ_RX, {}, { 50 });
        EXPECT_TRUE(stim.kick(VQ_RX));
        eth_frame tx(frame);
        eth_tx.send(tx);
        settle();
        ASSERT_EQ(stim.used[VQ_RX].size(), 1);
        EXPECT_EQ(stim.used[VQ_RX][0].length_out(), 0);

        EXPECT_TRUE(receive(frame));
    }

    void test_filter() {
        prepare();
        EXPECT_TRUE(receive(make_frame(mac, 64, 1)));
        EXPECT_TRUE(receive(make_frame(broadcast, 64, 2)));
        EXPECT_FALSE(receive(make_frame(other, 64, 3)));
        EXPECT_FALSE(receive(make_frame(multicast, 64, 4)));

        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_ALLMULTI, 1 }), CTRL_OK);
        EXPECT_TRUE(receive(make_frame(multicast, 64, 5)));
        EXPECT_FALSE(receive(make_frame(other, 64, 6)));
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_NOMULTI, 1 }), CTRL_OK);
        EXPECT_FALSE(receive(make_frame(multicast, 64, 7)));
        EXPECT_TRUE(receive(make_frame(broadcast, 64, 8)));
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_NOMULTI, 0 }), CTRL_OK);
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_ALLMULTI, 0 }), CTRL_OK);
        EXPECT_FALSE(receive(make_frame(multicast, 64, 9)));

        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_ALLUNI, 1 }), CTRL_OK);
        EXPECT_TRUE(receive(make_frame(other, 64, 10)));
        EXPECT_FALSE(receive(make_frame(multicast, 64, 11)));
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_NOUNI, 1 }), CTRL_OK);
        EXPECT_FALSE(receive(make_frame(other, 64, 12)));
        EXPECT_FALSE(receive(make_frame(mac, 64, 13)));
        EXPECT_TRUE(receive(make_frame(broadcast, 64, 14)));
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_NOUNI, 0 }), CTRL_OK);
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_ALLUNI, 0 }), CTRL_OK);
        EXPECT_FALSE(receive(make_frame(other, 64, 15)));

        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_NOBCAST, 1 }), CTRL_OK);
        EXPECT_FALSE(receive(make_frame(broadcast, 64, 16)));
        EXPECT_TRUE(receive(make_frame(mac, 64, 17)));
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_NOBCAST, 0 }), CTRL_OK);

        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_PROMISC, 1 }), CTRL_OK);
        EXPECT_TRUE(receive(make_frame(other, 64, 18)));
        EXPECT_TRUE(receive(make_frame(multicast, 64, 19)));
        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_PROMISC, 0 }), CTRL_OK);
        EXPECT_FALSE(receive(make_frame(other, 64, 20)));

        EXPECT_EQ(control({ CTRL_RX, CTRL_RX_PROMISC, 1 }), CTRL_OK);
        stim.identify();
        EXPECT_FALSE(receive(make_frame(other, 64, 21)));
    }

    void test_mac_address() {
        prepare();
        vector<u8> command = { CTRL_MAC, CTRL_MAC_SET };
        command.insert(command.end(), other.bytes.begin(), other.bytes.end());
        EXPECT_EQ(control(command), CTRL_OK);

        u8 addr[6] = {};
        EXPECT_TRUE(stim.read_config(0, addr));
        EXPECT_EQ(
            mac_addr(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]),
            other);
        EXPECT_TRUE(receive(make_frame(other, 64, 1)));
        EXPECT_FALSE(receive(make_frame(mac, 64, 2)));

        stim.identify();
        EXPECT_TRUE(receive(make_frame(mac, 64, 3)));
        EXPECT_FALSE(receive(make_frame(other, 64, 4)));
    }

    void test_mac_table() {
        prepare();
        const mac_addr multicast2("01:00:5e:00:00:02");
        vector<u8> command = { CTRL_MAC, CTRL_MAC_TABLE_SET, 2, 0, 0, 0 };
        command.insert(command.end(), other.bytes.begin(), other.bytes.end());
        command.insert(command.end(), peer.bytes.begin(), peer.bytes.end());
        command.insert(command.end(), { 1, 0, 0, 0 });
        command.insert(command.end(), multicast.bytes.begin(),
                       multicast.bytes.end());
        EXPECT_EQ(control(command), CTRL_OK);

        EXPECT_TRUE(receive(make_frame(other, 64, 1)));
        EXPECT_TRUE(receive(make_frame(peer, 64, 2)));
        EXPECT_TRUE(receive(make_frame(multicast, 64, 3)));
        EXPECT_TRUE(receive(make_frame(mac, 64, 4)));
        EXPECT_FALSE(receive(make_frame(multicast2, 64, 5)));

        EXPECT_EQ(
            control({ CTRL_MAC, CTRL_MAC_TABLE_SET, 0, 0, 0, 0, 0, 0, 0, 0 }),
            CTRL_OK);
        EXPECT_FALSE(receive(make_frame(other, 64, 6)));
        EXPECT_FALSE(receive(make_frame(multicast, 64, 7)));

        EXPECT_EQ(control({ CTRL_MAC, CTRL_MAC_TABLE_SET, 0xff, 0xff, 0xff,
                            0xff, 0x12, 0x34, 0x56, 0x00, 0x00, 0x01 }),
                  CTRL_ERR);
        EXPECT_FALSE(receive(make_frame(other, 64, 8)));
        EXPECT_EQ(control({ CTRL_MAC, CTRL_MAC_TABLE_SET, 0, 0 }), CTRL_ERR);
        stim.identify();
    }

    void test_control() {
        prepare();
        EXPECT_EQ(control({ CTRL_ANNOUNCE, 0 }), CTRL_OK);
        EXPECT_EQ(control({ CTRL_ANNOUNCE, 1 }), CTRL_ERR);
        EXPECT_EQ(control({ CTRL_MAC, 7 }), CTRL_ERR);
        EXPECT_EQ(control({ CTRL_RX, 99, 1 }), CTRL_OK);
        EXPECT_EQ(control({ CTRL_VLAN, 0, 1, 0 }), CTRL_NONE);
        EXPECT_TRUE(stim.kick(VQ_CTRL));

        stim.submit(VQ_CTRL, { { CTRL_ANNOUNCE, 0 } }, { 1 });
        stim.accept = false;
        EXPECT_TRUE(stim.kick(VQ_CTRL));
        stim.accept = true;
        EXPECT_EQ(stim.irqs, 0);
    }

    void test_link() {
        prepare();
        u16 status = 0;
        stringstream ss;
        EXPECT_TRUE(stim.read_config(6, status));
        EXPECT_EQ(status, 1);

        EXPECT_TRUE(net.execute("link_down", ss));
        EXPECT_EQ(stim.irqs, 1);
        EXPECT_TRUE(stim.read_config(6, status));
        EXPECT_EQ(status, 0);

        EXPECT_TRUE(net.execute("link_down", ss));
        EXPECT_EQ(stim.irqs, 1);
        stim.identify();
        EXPECT_TRUE(stim.read_config(6, status));
        EXPECT_EQ(status, 0);

        EXPECT_TRUE(net.execute("link_up", ss));
        EXPECT_EQ(stim.irqs, 2);
        EXPECT_TRUE(stim.read_config(6, status));
        EXPECT_EQ(status, 1);

        EXPECT_TRUE(net.execute("link_up", ss));
        EXPECT_EQ(stim.irqs, 2);
        EXPECT_TRUE(net.execute("link_status", ss));
    }
};

TEST(virtio, blk) {
    broker broker("test");
    broker.define("stim.net.mac", "12:34:56:78:9a:bc");

    virtio_net_stim stim("stim");
    sc_core::sc_start();
}
