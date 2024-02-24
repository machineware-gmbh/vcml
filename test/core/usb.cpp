/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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

MATCHER_P(usb_match_socket, name, "Matches an USB socket by name") {
    return strcmp(arg.basename(), name) == 0;
}

MATCHER_P(usb_match_packet, data, "Matches an USB packet by data") {
    return arg.length == 8 && *(u64*)arg.data == data;
}

class usb_bench : public test_base, public usb_host_if, public usb_dev_if
{
public:
    usb_initiator_socket usb_out;
    usb_base_initiator_socket usb_out_h;
    usb_base_target_socket usb_in_h;
    usb_target_socket usb_in;

    usb_initiator_array usb_array_out;
    usb_target_array usb_array_in;

    usb_bench(const sc_module_name& nm):
        test_base(nm),
        usb_host_if(),
        usb_dev_if(),
        usb_out("usb_out"),
        usb_out_h("usb_out_h"),
        usb_in_h("usb_in_h"),
        usb_in("usb_in"),
        usb_array_out("usb_array_out"),
        usb_array_in("usb_array_in") {
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
    }
};

TEST(usb, simulate) {
    usb_bench bench("system");
    sc_core::sc_start();
}
