/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsptest.h"

static void run_client(vspserver& session, test_vsp_cpu& cpu,
                       test_vsp_client& stay) {
    size_t suspends = 0;
    size_t without_bps = 0;

    // client 1 is going to disconnect while the simulation is running; it
    // leaves a breakpoint behind that its destructor must remove while the
    // cpu keeps iterating over its breakpoints
    {
        test_vsp_client leave(session.port()); // client 1
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 1); }));
        EXPECT_TRUE(starts_with(leave.command("mkbp,cpu,0x1000"), "OK"));
        without_bps = cpu.steps_without_bps; // cpu steps once at t=0

        // simulation only runs once all clients have resumed
        EXPECT_EQ(stay.command("resume"), "OK");
        EXPECT_EQ(leave.command("resume"), "OK");

        size_t steps = cpu.steps;
        ASSERT_TRUE(wait_for([&]() { return cpu.steps > steps + 100; }));
        EXPECT_EQ(cpu.steps_without_bps, without_bps);
        suspends = cpu.suspends;
    }

    // the server must pause the simulation to delete client 1, then resume
    // it again, because client 0 still wants to run
    ASSERT_TRUE(wait_for([&]() { return !has_client(session, 1); }));
    ASSERT_TRUE(
        wait_for([&]() { return cpu.steps_without_bps > without_bps + 100; }));
    EXPECT_GT(cpu.suspends, suspends) << "simulation not paused on disconnect";
    EXPECT_TRUE(has_client(session, 0));
}

TEST(vspserver, disconnect) {
    test_vsp_cpu cpu("cpu");
    vspserver session("localhost", 0);

    thread client([&]() {
        test_vsp_client stay(session.port()); // client 0
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));
        run_client(session, cpu, stay);
        cpu.done = true;

        // stay connected until the end, otherwise the server pauses the
        // simulation when its last client leaves
        EXPECT_TRUE(wait_for([&]() {
            return sc_core::sc_get_status() == sc_core::SC_STOPPED;
        }));
    });

    session.start();
    client.join();

    EXPECT_TRUE(cpu.breakpoints().empty());
}
