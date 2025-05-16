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

class lin_bench : public test_base, public lin_host, public can_host
{
public:
    lin::gateway gateway;
    lin::network network;

    lin_target_socket lin_in;
    can_initiator_socket can_out;

    virtual void lin_receive(const lin_target_socket& s,
                             lin_payload& tx) override {
        switch (tx.linid) {
        case 10:
            EXPECT_EQ(tx.data[0], 4);
            EXPECT_EQ(tx.data[1], 6);
            tx.status = LIN_SUCCESS;
            break;
        case 11:
            tx.data[0] = 1;
            tx.data[1] = 2;
            tx.status = LIN_SUCCESS;
        default:
            break;
        }
    }

    lin_bench(const sc_module_name& nm):
        test_base(nm),
        lin_host(),
        can_host(),
        gateway("gateway"),
        network("network"),
        lin_in("lin_in"),
        can_out("can_out") {
        can_out.bind(gateway.can_in);
        network.bind(gateway.lin_out);
        network.bind(lin_in);

        {
            // just to test model exporting works, not used later
            model export1("export1", "vcml::lin::gateway");
            model export2("export2", "vcml::lin::network");
        }

        add_test("strings", &lin_bench::test_strings);
        add_test("tx", &lin_bench::test_tx);
        add_test("rx", &lin_bench::test_rx);
        add_test("nodev", &lin_bench::test_nodev);
    };

    void test_strings() {
        EXPECT_STREQ(gateway.kind(), "vcml::lin::gateway");
        EXPECT_STREQ(network.kind(), "vcml::lin::network");
    }

    void test_tx() {
        can_frame frame{};
        frame.msgid = 10;
        frame.dlc = len2dlc(2);
        frame.data[0] = 4;
        frame.data[1] = 6;
        can_out.send(frame);
        EXPECT_FALSE(frame.is_err());
    }

    void test_rx() {
        can_frame frame{};
        frame.msgid = 11;
        frame.dlc = len2dlc(2);
        can_out.send(frame);
        ASSERT_FALSE(frame.is_err());
        EXPECT_EQ(frame.data[0], 1);
        EXPECT_EQ(frame.data[1], 2);
    }

    void test_nodev() {
        can_frame frame{};
        frame.msgid = 12;
        frame.dlc = len2dlc(2);
        can_out.send(frame);
        EXPECT_TRUE(frame.is_err());
    }
};

TEST(lin, gateway) {
    lin_bench test("test");
    sc_core::sc_start();
}
