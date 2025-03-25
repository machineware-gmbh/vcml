/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

using namespace sc_dt;

TEST(signal, payload) {
    signal_payload<u64> tx;
    tx.data = 42;

    // no formatting checks, just make sure it compiles
    std::cout << tx << std::endl;
}

TEST(signal, payload_bigint) {
    signal_payload<sc_biguint<128>> tx("0xaaaaaaaabbbbbbbbccccccccdddddddd");

    // no formatting checks, just make sure it compiles
    std::cout << tx << std::endl;
}

MATCHER_P(signal_s, name, "Matches a signal socket by name") {
    return strcmp(arg.basename(), name) == 0;
}

class signal_test : public test_base,
                    public signal_host<u64>,
                    public signal_host<sc_biguint<256>>
{
public:
    MOCK_METHOD(void, signal_transport,
                (const signal_target_socket<u64>&, const u64&), (override));
    MOCK_METHOD(void, signal_transport,
                (const signal_target_socket<sc_biguint<256>>&,
                 const sc_biguint<256>&),
                (override));

    signal_initiator_socket<u64> out64;
    signal_base_initiator_socket<u64> outh64;
    signal_base_target_socket<u64> inh64;
    signal_target_socket<u64> in64;

    signal_initiator_socket<u64> out64s;
    signal_initiator_socket<u64> out64sig;
    signal_target_array<u64, 14> in64s;

    signal_initiator_socket<sc_biguint<256>> out256;
    signal_base_initiator_socket<sc_biguint<256>> outh256;
    signal_base_target_socket<sc_biguint<256>> inh256;
    signal_target_array<sc_biguint<256>, 8> in256x8;

    sc_signal<u64> signal64;
    sc_signal<sc_biguint<256>> signal256;

    signal_test(const sc_module_name& nm):
        test_base(nm),
        signal_host<u64>(),
        signal_host<sc_biguint<256>>(),
        out64("out64"),
        outh64("outh64"),
        inh64("inh64"),
        in64("in64"),
        out64s("out64s"),
        out64sig("out64sig"),
        in64s("in64s"),
        out256("out256"),
        outh256("outh256"),
        inh256("inh256"),
        in256x8("in256x8"),
        signal64("signal64"),
        signal256("signal256") {
        EXPECT_STREQ(out64.kind(), "vcml::signal_initiator_socket");
        EXPECT_STREQ(outh64.kind(), "vcml::signal_base_initiator_socket");
        EXPECT_STREQ(inh64.kind(), "vcml::signal_base_target_socket");
        EXPECT_STREQ(in64.kind(), "vcml::signal_target_socket");

        // test binding
        signal_bind(*this, "out64", *this, "outh64");
        signal_bind(*this, "inh64", *this, "in64");
        signal_bind(*this, "outh64", *this, "inh64");

        // test stubbing
        signal_stub(*this, "out64s");
        signal_stub(*this, "in64s", 13);
        EXPECT_TRUE(find_object("test.out64s_stub"));
        EXPECT_TRUE(find_object("test.in64s[13]_stub"));

        // test signal binding
        out64sig.bind(signal64);
        EXPECT_TRUE(find_object("test.out64sig_adapter"));

        // test binding 256 bit
        out256.bind(in256x8[2]);
        out256.bind(outh256);
        inh256.bind(in256x8[4]);
        outh256.bind(inh256);

        // test signal binding
        in256x8[1].bind(signal256);
        EXPECT_TRUE(find_object("test.in256x8[1]_adapter"));

        // test stubbing 256 bit
        in256x8[6].stub();
        in256x8[7].stub();
        EXPECT_TRUE(find_object("test.in256x8[6]_stub"));
        EXPECT_TRUE(find_object("test.in256x8[7]_stub"));

        add_test("signal_u64", &signal_test::test_signal_u64);
        add_test("signal_u256", &signal_test::test_signal_u256);
    }

    void test_signal_u64() {
        EXPECT_CALL(*this, signal_transport(signal_s("in64"), 123));
        out64 = 123;
        EXPECT_EQ(in64, 123);

        // wirting the same value again should not trigger a transport
        EXPECT_CALL(*this, signal_transport(signal_s("in64"), 123)).Times(0);
        out64 = 123;

        // test driving a signal via an initiator
        out64sig = 432;
        wait(signal64.default_event());
        EXPECT_EQ(signal64, 432);
    }

    void test_signal_u256() {
        sc_biguint<256> a("0xaaaaaaaabbbbbbbbccccccccddddddddeeeeeeee");
        EXPECT_CALL(*this, signal_transport(signal_s("in256x8[2]"), a));
        EXPECT_CALL(*this, signal_transport(signal_s("in256x8[4]"), a));
        out256 = a;
        EXPECT_EQ(in256x8[4], a);

        // writing the same value should not trigger a transport
        EXPECT_CALL(*this, signal_transport(signal_s("in256x8[2]"), a))
            .Times(0);
        EXPECT_CALL(*this, signal_transport(signal_s("in256x8[4]"), a))
            .Times(0);
        out256 = a;
        EXPECT_EQ(in256x8[4], a);

        // test notification via signal
        sc_biguint<256> b("0x1111111122222222333333334444444455555555");
        EXPECT_CALL(*this, signal_transport(signal_s("in256x8[1]"), b));
        signal256 = b;
        wait(SC_ZERO_TIME);
        EXPECT_EQ(in256x8[1], b);
    }
};

TEST(signal, sockets) {
    signal_test test("test");
    sc_core::sc_start();
}
