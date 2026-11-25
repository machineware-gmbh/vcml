/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class eth_network_test : public test_base, public eth_host
{
public:
    enum : size_t { NODES = 3 };

    ethernet::network network;
    eth_initiator_array<NODES> eth_tx;
    eth_target_array<NODES> eth_rx;
    vector<eth_frame> received[NODES];

    virtual void eth_receive(const eth_target_socket& socket,
                             const eth_frame& frame) override {
        received[eth_rx.index_of(socket)].push_back(frame);
    }

    eth_network_test(const sc_module_name& nm):
        test_base(nm),
        eth_host(),
        network("network"),
        eth_tx("eth_tx"),
        eth_rx("eth_rx"),
        received() {
        for (size_t i = 0; i < NODES; i++)
            network.bind(eth_tx[i], eth_rx[i]);

        add_test("strings", &eth_network_test::test_strings);
        add_test("broadcast", &eth_network_test::test_broadcast);
    }

    static eth_frame make_frame(u8 id, const vector<u8>& data) {
        mac_addr dest(0x12, 0x34, 0x56, 0x78, 0x9a, id);
        mac_addr src(0xaa, 0xbb, 0xcc, 0xdd, 0xee, id);
        return eth_frame(dest, src, data);
    }

    void test_strings() {
        EXPECT_STREQ(network.kind(), "vcml::ethernet::network");
        EXPECT_STREQ(network.version(), VCML_VERSION_STRING);
    }

    void test_broadcast() {
        eth_frame frame0 = make_frame(1, { 1, 2, 3 });
        eth_frame frame1 = make_frame(2, { 4 });
        eth_frame frame2 = make_frame(3, {});

        eth_frame tx = frame0;
        eth_tx[0].send(tx);
        EXPECT_TRUE(received[0].empty());
        EXPECT_THAT(received[1], ElementsAre(frame0));
        EXPECT_THAT(received[2], ElementsAre(frame0));

        tx = frame1;
        eth_tx[1].send(tx);
        EXPECT_THAT(received[0], ElementsAre(frame1));
        EXPECT_THAT(received[1], ElementsAre(frame0));
        EXPECT_THAT(received[2], ElementsAre(frame0, frame1));

        tx = frame2;
        eth_tx[2].send(tx);
        EXPECT_THAT(received[0], ElementsAre(frame1, frame2));
        EXPECT_THAT(received[1], ElementsAre(frame0, frame2));
        EXPECT_THAT(received[2], ElementsAre(frame0, frame1));
    }
};

TEST(ethernet, network) {
    eth_network_test test("test");
    sc_core::sc_start();
}
