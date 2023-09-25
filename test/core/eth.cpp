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

#include "vcml/protocols/eth.h"

TEST(ethernet, macaddr) {
    mac_addr addr("12:23:34:45:56:67");

    EXPECT_EQ(addr[0], 0x12);
    EXPECT_EQ(addr[1], 0x23);
    EXPECT_EQ(addr[2], 0x34);
    EXPECT_EQ(addr[3], 0x45);
    EXPECT_EQ(addr[4], 0x56);
    EXPECT_EQ(addr[5], 0x67);
    EXPECT_EQ((u64)addr, 0x122334455667ul);
}

TEST(ethernet, to_string) {
    vector<u8> data = { 0x11, 0x22, 0x33, 0x44 };
    eth_frame frame("ff:ff:ff:ff:ff:ff", "12:23:34:45:56:67", data);

    stringstream ss;
    ss << frame;
    std::cout << frame << std::endl;
}

TEST(ethernet, frame) {
    vector<u8> data = { 0x11, 0x22, 0x33, 0x44 };
    eth_frame frame("ff:ff:ff:ff:ff:ff", "12:23:34:45:56:67", data);

    EXPECT_EQ(frame.destination(), "ff:ff:ff:ff:ff:ff");
    EXPECT_EQ(frame.source(), "12:23:34:45:56:67");

    ASSERT_GE(frame.payload_size(), data.size());
    for (size_t i = 0; i < data.size(); i++)
        EXPECT_EQ(frame.payload(i), data[i]);

    EXPECT_TRUE(success(frame));
    EXPECT_FALSE(failed(frame));
}

MATCHER_P(eth_match_socket, socket, "Matches an ethernet socket") {
    return &arg == socket;
}

MATCHER_P(eth_match_frame, frame, "Matches an ethernet frame") {
    return arg == frame;
}

class ethernet_bench : public test_base, public eth_host
{
public:
    eth_initiator_socket eth_tx;
    eth_base_initiator_socket eth_tx_h;
    eth_base_target_socket eth_rx_h;
    eth_target_socket eth_rx;

    eth_initiator_array eth_array_tx;
    eth_target_array eth_array_rx;

    ethernet_bench(const sc_module_name& nm):
        test_base(nm),
        eth_host(),
        eth_tx("eth_tx"),
        eth_tx_h("eth_tx_h"),
        eth_rx_h("eth_rx_h"),
        eth_rx("eth_rx"),
        eth_array_tx("eth_array_tx"),
        eth_array_rx("eth_array_rx") {
        eth_bind(*this, "eth_tx", *this, "eth_tx_h");
        eth_bind(*this, "eth_rx_h", *this, "eth_rx");
        eth_bind(*this, "eth_tx_h", *this, "eth_rx_h");

        eth_bind(*this, "eth_array_tx", 4, *this, "eth_array_rx", 4);
        eth_stub(*this, "eth_array_tx", 5);
        eth_stub(*this, "eth_array_rx", 6);

        // did the ports get created?
        EXPECT_TRUE(find_object("eth.eth_array_tx[4]"));
        EXPECT_TRUE(find_object("eth.eth_array_rx[4]"));
        EXPECT_TRUE(find_object("eth.eth_array_tx[5]"));
        EXPECT_TRUE(find_object("eth.eth_array_rx[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("eth.eth_array_tx[5]_stub"));
        EXPECT_TRUE(find_object("eth.eth_array_rx[6]_stub"));
    }

    MOCK_METHOD(void, eth_receive,
                (const eth_target_socket&, const eth_frame&), (override));

    MOCK_METHOD(void, eth_link_up, (), (override));
    MOCK_METHOD(void, eth_link_down, (), (override));

    virtual void run_test() override {
        wait(SC_ZERO_TIME);

        stringstream ss;
        vector<u8> data = { 0x11, 0x22, 0x33, 0x44 };
        eth_frame frame("ff:ff:ff:ff:ff:ff", "12:23:34:45:56:67", data);

        EXPECT_CALL(*this, eth_receive(_, eth_match_frame(frame)));
        eth_tx.send(frame);

        EXPECT_CALL(*this, eth_link_down());
        EXPECT_TRUE(execute("link_down", {}, ss));
        EXPECT_EQ(ss.str(), "");

        EXPECT_CALL(*this, eth_receive(_, _)).Times(0);
        eth_tx.send(frame);

        EXPECT_CALL(*this, eth_link_up()).Times(1);
        EXPECT_TRUE(execute("link_up", {}, ss));
        EXPECT_TRUE(execute("link_up", {}, ss)); // should not trigger
        EXPECT_EQ(ss.str(), "");

        EXPECT_CALL(*this, eth_receive(_, eth_match_frame(frame)));
        eth_tx.send(frame);
    }
};

TEST(ethernet, simulate) {
    ethernet_bench bench("eth");
    sc_core::sc_start();
}
