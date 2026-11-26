/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(usb, speed) {
    EXPECT_STREQ(usb_speed_str(USB_SPEED_LOW), "USB_SPEED_LOW");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_FULL), "USB_SPEED_FULL");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_HIGH), "USB_SPEED_HIGH");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_SUPER), "USB_SPEED_SUPER");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_NONE), "USB_SPEED_NONE");
}

TEST(usb, packet) {
    u32 data = 0x1234cd78;
    usb_packet p;
    p.token = USB_TOKEN_IN;
    p.addr = 1;
    p.epno = 2;
    p.result = USB_RESULT_SUCCESS;
    p.data = (u8*)&data;
    p.length = sizeof(data);
    auto str = to_string(p);
    EXPECT_EQ(str, "USB_TOKEN_IN @ 1.2 [78 cd 34 12] (USB_RESULT_SUCCESS)");

    p.token = USB_TOKEN_OUT;
    p.addr = 7;
    p.epno = 5;
    p.length = 0;
    p.result = USB_RESULT_NACK;
    str = to_string(p);
    EXPECT_EQ(str, "USB_TOKEN_OUT @ 7.5 [<no data>] (USB_RESULT_NACK)");
}

TEST(usb, release) {
    EXPECT_STREQ(usb_release_str(USB_1_0), "USB 1.0");
    EXPECT_STREQ(usb_release_str(USB_1_1), "USB 1.1");
    EXPECT_STREQ(usb_release_str(USB_2_0), "USB 2.0");
    EXPECT_STREQ(usb_release_str(USB_2_1), "USB 2.1");
    EXPECT_STREQ(usb_release_str(USB_3_0), "USB 3.0");
    EXPECT_STREQ(usb_release_str(USB_3_1), "USB 3.1");
    EXPECT_STREQ(usb_release_str(USB_3_2), "USB 3.2");
    EXPECT_STREQ(usb_release_str(USB_4_0), "USB 4.0");
    EXPECT_STREQ(usb_release_str(0x123), "unknown USB release");
}

TEST(usb, strings) {
    EXPECT_STREQ(usb_token_str(USB_TOKEN_IN), "USB_TOKEN_IN");
    EXPECT_STREQ(usb_token_str(USB_TOKEN_OUT), "USB_TOKEN_OUT");
    EXPECT_STREQ(usb_token_str(USB_TOKEN_SETUP), "USB_TOKEN_SETUP");

    EXPECT_STREQ(usb_result_str(USB_RESULT_SUCCESS), "USB_RESULT_SUCCESS");
    EXPECT_STREQ(usb_result_str(USB_RESULT_INCOMPLETE),
                 "USB_RESULT_INCOMPLETE");
    EXPECT_STREQ(usb_result_str(USB_RESULT_NODEV), "USB_RESULT_NODEV");
    EXPECT_STREQ(usb_result_str(USB_RESULT_NACK), "USB_RESULT_NACK");
    EXPECT_STREQ(usb_result_str(USB_RESULT_STALL), "USB_RESULT_STALL");
    EXPECT_STREQ(usb_result_str(USB_RESULT_BABBLE), "USB_RESULT_BABBLE");
    EXPECT_STREQ(usb_result_str(USB_RESULT_IOERROR), "USB_RESULT_IOERROR");

    EXPECT_STREQ(usb_desc_str(USB_DT_DEVICE), "USB_DT_DEVICE");
    EXPECT_STREQ(usb_desc_str(USB_DT_CONFIG), "USB_DT_CONFIG");
    EXPECT_STREQ(usb_desc_str(USB_DT_STRING), "USB_DT_STRING");
    EXPECT_STREQ(usb_desc_str(USB_DT_INTERFACE), "USB_DT_INTERFACE");
    EXPECT_STREQ(usb_desc_str(USB_DT_ENDPOINT), "USB_DT_ENDPOINT");
    EXPECT_STREQ(usb_desc_str(USB_DT_DEVICE_QUALIFIER),
                 "USB_DT_DEVICE_QUALIFIER");
    EXPECT_STREQ(usb_desc_str(USB_DT_OTHER_SPEED_CONFIG),
                 "USB_DT_OTHER_SPEED_CONFIG");
    EXPECT_STREQ(usb_desc_str(USB_DT_DEBUG), "USB_DT_DEBUG");
    EXPECT_STREQ(usb_desc_str(USB_DT_INTERFACE_ASSOC),
                 "USB_DT_INTERFACE_ASSOC");
    EXPECT_STREQ(usb_desc_str(USB_DT_BOS), "USB_DT_BOS");
    EXPECT_STREQ(usb_desc_str(USB_DT_DEVICE_CAPABILITY),
                 "USB_DT_DEVICE_CAPABILITY");
    EXPECT_STREQ(usb_desc_str(USB_DT_HID), "USB_DT_HID");
    EXPECT_STREQ(usb_desc_str(USB_DT_REPORT), "USB_DT_REPORT");
    EXPECT_STREQ(usb_desc_str(USB_DT_PHYSICAL), "USB_DT_PHYSICAL");
    EXPECT_STREQ(usb_desc_str(USB_DT_CS_INTERFACE), "USB_DT_CS_INTERFACE");
    EXPECT_STREQ(usb_desc_str(USB_DT_CS_ENDPOINT), "USB_DT_CS_ENDPOINT");
    EXPECT_STREQ(usb_desc_str(USB_DT_ENDPOINT_COMPANION),
                 "USB_DT_ENDPOINT_COMPANION");
    EXPECT_STREQ(usb_desc_str(0x42), "USB_DT_UNKNOWN");

    EXPECT_STREQ(usb_endpoint_str(USB_EP_CTRL), "USB_EP_CTRL");
    EXPECT_STREQ(usb_endpoint_str(USB_EP_ISOC), "USB_EP_ISOC");
    EXPECT_STREQ(usb_endpoint_str(USB_EP_BULK), "USB_EP_BULK");
    EXPECT_STREQ(usb_endpoint_str(USB_EP_IRQ), "USB_EP_IRQ");
    EXPECT_STREQ(usb_endpoint_str(USB_EP_ISOC | USB_EP_ISOC_ASYNC |
                                  USB_EP_ISOC_FEEDBACK),
                 "USB_EP_ISOC");
    EXPECT_STREQ(usb_endpoint_str(USB_EP_IRQ | USB_EP_IRQ_NOTIFICATION),
                 "USB_EP_IRQ");
}

TEST(usb, packet_helpers) {
    u8 buf[8] = { 0x2d };

    usb_packet p = usb_packet_setup(3, buf, sizeof(buf));
    EXPECT_EQ(p.addr, 3u);
    EXPECT_EQ(p.epno, 0u);
    EXPECT_EQ(p.token, USB_TOKEN_SETUP);
    EXPECT_EQ(p.result, USB_RESULT_INCOMPLETE);
    EXPECT_EQ(p.data, buf);
    EXPECT_EQ(p.length, sizeof(buf));

    p = usb_packet_in(4, 2, buf, 5);
    EXPECT_EQ(p.addr, 4u);
    EXPECT_EQ(p.epno, 2u);
    EXPECT_EQ(p.token, USB_TOKEN_IN);
    EXPECT_EQ(p.result, USB_RESULT_INCOMPLETE);
    EXPECT_EQ(p.data, buf);
    EXPECT_EQ(p.length, 5u);

    p = usb_packet_out(5, 1, buf, 3);
    EXPECT_EQ(p.addr, 5u);
    EXPECT_EQ(p.epno, 1u);
    EXPECT_EQ(p.token, USB_TOKEN_OUT);
    EXPECT_EQ(p.result, USB_RESULT_INCOMPLETE);
    EXPECT_EQ(p.data, buf);
    EXPECT_EQ(p.length, 3u);

    p = usb_packet_setup(3, buf, 1);
    EXPECT_EQ(to_string(p),
              "USB_TOKEN_SETUP @ 3.0 [2d] (USB_RESULT_INCOMPLETE)");

    p.length = 0;
    p.data = nullptr;
    const usb_result results[] = {
        USB_RESULT_SUCCESS, USB_RESULT_INCOMPLETE, USB_RESULT_NODEV,
        USB_RESULT_NACK,    USB_RESULT_STALL,      USB_RESULT_BABBLE,
        USB_RESULT_IOERROR,
    };

    for (usb_result res : results) {
        p.result = res;
        EXPECT_EQ(to_string(p), mkstr("USB_TOKEN_SETUP @ 3.0 [<no data>] (%s)",
                                      usb_result_str(res)));
    }
}

TEST(usb, results) {
    EXPECT_TRUE(success(USB_RESULT_SUCCESS));
    EXPECT_FALSE(failed(USB_RESULT_SUCCESS));
    EXPECT_FALSE(success(USB_RESULT_INCOMPLETE));
    EXPECT_FALSE(failed(USB_RESULT_INCOMPLETE));

    const usb_result errors[] = {
        USB_RESULT_NODEV,  USB_RESULT_NACK,    USB_RESULT_STALL,
        USB_RESULT_BABBLE, USB_RESULT_IOERROR,
    };

    for (usb_result res : errors) {
        EXPECT_FALSE(success(res)) << usb_result_str(res);
        EXPECT_TRUE(failed(res)) << usb_result_str(res);
    }

    usb_packet p = usb_packet_in(1, 1, nullptr, 0);
    EXPECT_FALSE(success(p));
    EXPECT_FALSE(failed(p));
    p.result = USB_RESULT_SUCCESS;
    EXPECT_TRUE(success(p));
    EXPECT_FALSE(failed(p));
    p.result = USB_RESULT_STALL;
    EXPECT_FALSE(success(p));
    EXPECT_TRUE(failed(p));

    EXPECT_EQ(usb_ep_in(1), 0x81);
    EXPECT_EQ(usb_ep_in(0x81), 0x81);
    EXPECT_EQ(usb_ep_out(2), 0x02);
    EXPECT_EQ(usb_ep_out(15), 0x0f);
}

TEST(usb, speed_max) {
    const std::pair<u16, usb_speed> speeds[] = {
        { 0x0000, USB_SPEED_NONE },   { USB_1_0, USB_SPEED_LOW },
        { USB_1_1, USB_SPEED_FULL },  { 0x0120, USB_SPEED_NONE },
        { USB_2_0, USB_SPEED_HIGH },  { 0x0201, USB_SPEED_HIGH },
        { USB_2_1, USB_SPEED_HIGH },  { USB_3_0, USB_SPEED_SUPER },
        { USB_3_1, USB_SPEED_SUPER }, { USB_3_2, USB_SPEED_SUPER },
        { USB_4_0, USB_SPEED_NONE },
    };

    usb::device_desc desc{};
    for (auto [bcd, speed] : speeds) {
        desc.bcd_usb = bcd;
        EXPECT_EQ(usb::usb_speed_max(desc), speed) << usb_release_str(bcd);
    }
}

MATCHER_P(usb_match_socket, name, "Matches an USB socket by name") {
    return strcmp(arg.basename(), name) == 0;
}

MATCHER_P(usb_match_packet, data, "Matches an USB packet by data") {
    return arg.length == 8 && *(u64*)arg.data == data;
}

class usb_socket_dev : public module, public usb_dev_if
{
public:
    usb_target_socket usb_in_a;
    usb_target_socket usb_in_b;

    usb_socket_dev(const sc_module_name& nm):
        module(nm), usb_dev_if(), usb_in_a("usb_in_a"), usb_in_b("usb_in_b") {}

    MOCK_METHOD(void, usb_reset_device, (const usb_target_socket&),
                (override));
    MOCK_METHOD(void, usb_reset_endpoint, (const usb_target_socket&, int),
                (override));
};

class usb_legacy_dev : public module, public usb_dev_if
{
public:
    usb_target_socket usb_in;

    usb_legacy_dev(const sc_module_name& nm):
        module(nm), usb_dev_if(), usb_in("usb_in") {}

    MOCK_METHOD(void, usb_reset_device, (), (override));
    MOCK_METHOD(void, usb_reset_endpoint, (int), (override));
};

class usb_bench : public test_base, public usb_host_if, public usb_dev_if
{
public:
    usb_initiator_socket usb_out;
    usb_base_initiator_socket usb_out_h;
    usb_base_target_socket usb_in_h;
    usb_target_socket usb_in;

    usb_initiator_array<> usb_array_out;
    usb_target_array<> usb_array_in;

    usb_initiator_socket usb_out_a;
    usb_initiator_socket usb_out_b;
    usb_initiator_socket usb_out_legacy;

    usb_socket_dev sockdev;
    usb_legacy_dev legacydev;

    usb_bench(const sc_module_name& nm):
        test_base(nm),
        usb_host_if(),
        usb_dev_if(),
        usb_out("usb_out"),
        usb_out_h("usb_out_h"),
        usb_in_h("usb_in_h"),
        usb_in("usb_in"),
        usb_array_out("usb_array_out"),
        usb_array_in("usb_array_in"),
        usb_out_a("usb_out_a"),
        usb_out_b("usb_out_b"),
        usb_out_legacy("usb_out_legacy"),
        sockdev("sockdev"),
        legacydev("legacydev") {
        usb_out_a.bind(sockdev.usb_in_a);
        usb_out_b.bind(sockdev.usb_in_b);
        usb_out_legacy.bind(legacydev.usb_in);

        usb_bind(*this, "usb_out", *this, "usb_out_h");
        usb_bind(*this, "usb_in_h", *this, "usb_in");
        usb_bind(*this, "usb_out_h", *this, "usb_in_h");

        usb_bind(*this, "usb_array_out", 4, *this, "usb_array_in", 4);
        usb_stub(*this, "usb_array_out", 5);
        usb_stub(*this, "usb_array_in", 6);

        // did the ports get created?
        EXPECT_TRUE(find_object("system.usb_array_out[4]"));
        EXPECT_TRUE(find_object("system.usb_array_in[4]"));
        EXPECT_TRUE(find_object("system.usb_array_out[5]"));
        EXPECT_TRUE(find_object("system.usb_array_in[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("system.usb_array_out[5]_stub"));
        EXPECT_TRUE(find_object("system.usb_array_in[6]_stub"));
        EXPECT_TRUE(usb_array_out[5].is_stubbed());
        EXPECT_TRUE(usb_array_in[6].is_stubbed());

        // correct kind provided?
        EXPECT_STREQ(usb_out.kind(), "vcml::usb_initiator_socket");
        EXPECT_STREQ(usb_out_h.kind(), "vcml::usb_base_initiator_socket");
        EXPECT_STREQ(usb_in_h.kind(), "vcml::usb_base_target_socket");
        EXPECT_STREQ(usb_in.kind(), "vcml::usb_target_socket");
    }

    MOCK_METHOD(void, usb_attach, (usb_initiator_socket&), (override));
    MOCK_METHOD(void, usb_detach, (usb_initiator_socket & s), (override));

    MOCK_METHOD(void, usb_reset_device, (), (override));
    MOCK_METHOD(void, usb_reset_endpoint, (int), (override));
    MOCK_METHOD(void, usb_transport, (const usb_target_socket&, usb_packet&),
                (override));

    virtual void run_test() override {
        wait(SC_ZERO_TIME);

        u64 data = 0x1122334455667788;
        usb_packet p = usb_packet_in(1, 0, &data, sizeof(data));

        // nothing should be received while disconnected
        EXPECT_CALL(*this, usb_transport(_, _)).Times(0);
        EXPECT_CALL(*this, usb_reset_device()).Times(0);
        EXPECT_CALL(*this, usb_reset_endpoint(_)).Times(0);
        usb_out.send(p);
        usb_out.reset_device();
        usb_out.reset_endpoint(10);
        EXPECT_EQ(p.result, USB_RESULT_NACK);

        // attach and re-send
        EXPECT_CALL(*this, usb_attach(usb_match_socket("usb_out")));
        EXPECT_CALL(*this, usb_transport(usb_match_socket("usb_in"),
                                         usb_match_packet(data)));
        usb_in.attach(USB_SPEED_SUPER);
        usb_out->usb_transport(p);
        EXPECT_TRUE(usb_out.is_attached());
        EXPECT_EQ(usb_out.connection_speed(), USB_SPEED_SUPER);

        // test resetting
        EXPECT_CALL(*this, usb_reset_device());
        usb_out->usb_reset(-1);
        EXPECT_CALL(*this, usb_reset_endpoint(0));
        usb_out->usb_reset(0);

        // test disconnecting
        EXPECT_CALL(*this, usb_detach(usb_match_socket("usb_out")));
        usb_in.detach();
        EXPECT_FALSE(usb_in.is_attached());
        EXPECT_FALSE(usb_out.is_attached());
        EXPECT_EQ(usb_in.connection_speed(), USB_SPEED_NONE);
        EXPECT_EQ(usb_out.connection_speed(), USB_SPEED_NONE);

        test_socket_resets();
        test_legacy_resets();
    }

    void test_socket_resets() {
        // resets on detached sockets must be dropped
        EXPECT_CALL(sockdev, usb_reset_device(_)).Times(0);
        EXPECT_CALL(sockdev, usb_reset_endpoint(_, _)).Times(0);
        usb_out_a.reset_device();
        usb_out_b.reset_endpoint(1);
        testing::Mock::VerifyAndClearExpectations(&sockdev);

        EXPECT_CALL(*this, usb_attach(usb_match_socket("usb_out_a")));
        EXPECT_CALL(*this, usb_attach(usb_match_socket("usb_out_b")));
        sockdev.usb_in_a.attach(USB_SPEED_HIGH);
        sockdev.usb_in_b.attach(USB_SPEED_HIGH);

        // each reset must arrive on the socket it was sent to
        EXPECT_CALL(sockdev, usb_reset_device(usb_match_socket("usb_in_a")));
        usb_out_a.reset_device();
        EXPECT_CALL(sockdev, usb_reset_device(usb_match_socket("usb_in_b")));
        usb_out_b.reset_device();
        EXPECT_CALL(sockdev,
                    usb_reset_endpoint(usb_match_socket("usb_in_a"), 2));
        usb_out_a.reset_endpoint(2);
        EXPECT_CALL(sockdev,
                    usb_reset_endpoint(usb_match_socket("usb_in_b"), 3));
        usb_out_b.reset_endpoint(3);
        testing::Mock::VerifyAndClearExpectations(&sockdev);

        EXPECT_CALL(*this, usb_detach(usb_match_socket("usb_out_a")));
        EXPECT_CALL(*this, usb_detach(usb_match_socket("usb_out_b")));
        sockdev.usb_in_a.detach();
        sockdev.usb_in_b.detach();

        // resets after detaching must be dropped again
        EXPECT_CALL(sockdev, usb_reset_device(_)).Times(0);
        EXPECT_CALL(sockdev, usb_reset_endpoint(_, _)).Times(0);
        usb_out_a.reset_device();
        usb_out_b.reset_endpoint(1);
    }

    void test_legacy_resets() {
        // resets on detached sockets must be dropped
        EXPECT_CALL(legacydev, usb_reset_device()).Times(0);
        EXPECT_CALL(legacydev, usb_reset_endpoint(_)).Times(0);
        usb_out_legacy.reset_device();
        usb_out_legacy.reset_endpoint(1);
        testing::Mock::VerifyAndClearExpectations(&legacydev);

        EXPECT_CALL(*this, usb_attach(usb_match_socket("usb_out_legacy")));
        legacydev.usb_in.attach(USB_SPEED_FULL);

        // socket-less overloads must still be reached via default forwarding
        EXPECT_CALL(legacydev, usb_reset_device());
        usb_out_legacy.reset_device();
        EXPECT_CALL(legacydev, usb_reset_endpoint(4));
        usb_out_legacy.reset_endpoint(4);
        testing::Mock::VerifyAndClearExpectations(&legacydev);

        EXPECT_CALL(*this, usb_detach(usb_match_socket("usb_out_legacy")));
        legacydev.usb_in.detach();
    }
};

TEST(usb, simulate) {
    usb_bench bench("system");
    sc_core::sc_start();
}
