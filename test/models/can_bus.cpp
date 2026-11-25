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

class can_bus_test : public test_base, public can_host
{
public:
    enum : size_t { NODES = 3 };

    can::bus bus;
    can_initiator_array<NODES> can_tx;
    can_target_array<NODES> can_rx;
    vector<can_frame> received[NODES];

    virtual void can_receive(const can_target_socket& socket,
                             can_frame& frame) override {
        received[can_rx.index_of(socket)].push_back(frame);
    }

    can_bus_test(const sc_module_name& nm):
        test_base(nm),
        can_host(),
        bus("bus"),
        can_tx("can_tx"),
        can_rx("can_rx"),
        received() {
        for (size_t i = 0; i < NODES; i++)
            bus.bind(can_tx[i], can_rx[i]);

        add_test("strings", &can_bus_test::test_strings);
        add_test("broadcast", &can_bus_test::test_broadcast);
    }

    static can_frame make_frame(u32 id, vector<u8> data) {
        can_frame frame{};
        frame.canid = id;
        frame.dlc = (u16)data.size();
        frame.data = std::move(data);
        return frame;
    }

    void test_strings() {
        EXPECT_STREQ(bus.kind(), "vcml::can::bus");
        EXPECT_STREQ(bus.version(), VCML_VERSION_STRING);
    }

    void test_broadcast() {
        can_frame frame0 = make_frame(0x100, { 1, 2, 3 });
        can_frame frame1 = make_frame(0x200, { 4 });
        can_frame frame2 = make_frame(0x300, {});

        can_frame tx = frame0;
        can_tx[0].send(tx);
        EXPECT_TRUE(received[0].empty());
        EXPECT_THAT(received[1], ElementsAre(frame0));
        EXPECT_THAT(received[2], ElementsAre(frame0));

        tx = frame1;
        can_tx[1].send(tx);
        EXPECT_THAT(received[0], ElementsAre(frame1));
        EXPECT_THAT(received[1], ElementsAre(frame0));
        EXPECT_THAT(received[2], ElementsAre(frame0, frame1));

        tx = frame2;
        can_tx[2].send(tx);
        EXPECT_THAT(received[0], ElementsAre(frame1, frame2));
        EXPECT_THAT(received[1], ElementsAre(frame0, frame2));
        EXPECT_THAT(received[2], ElementsAre(frame0, frame1));
    }
};

TEST(can, bus) {
    can_bus_test test("test");
    sc_core::sc_start();
}
