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

// status response: OK,stopped:<reason>,<time_ns>,<delta>
static u64 status_time(const string& status) {
    vector<string> args = split(status, ',');
    return args.size() > 2 ? strtoull(args[2].c_str(), nullptr, 10) : 0;
}

static bool is_stopped(const string& status) {
    return starts_with(status, "OK,stopped:");
}

static void run_client(vspserver& session, test_vsp_cpu& cpu) {
    // client 0 leaves while the simulation runs without a time limit
    {
        test_vsp_client leave(session.port()); // client 0
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));
        EXPECT_EQ(leave.command("resume"), "OK");

        size_t steps = cpu.steps;
        ASSERT_TRUE(wait_for([&]() { return cpu.steps > steps + 100; }));
    }

    ASSERT_TRUE(wait_for([&]() { return !has_client(session, 0); }));

    // client 1 connects later and must be able to step for a given time,
    // instead of continuing the unlimited run started by client 0
    test_vsp_client vsp(session.port()); // client 1
    ASSERT_TRUE(wait_for([&]() { return has_client(session, 1); }));

    string status = vsp.command("status");
    ASSERT_TRUE(is_stopped(status)) << status;
    u64 start = status_time(status);

    EXPECT_EQ(vsp.command("resume,1000ns"), "OK");
    EXPECT_TRUE(wait_for([&]() { return is_stopped(vsp.command("status")); }))
        << "step did not complete";

    status = vsp.command("status");
    EXPECT_TRUE(starts_with(status, "OK,stopped:step")) << status;
    EXPECT_EQ(status_time(status), start + 1000) << status;

    // let the simulation finish, stay connected until it has ended
    cpu.done = true;
    EXPECT_EQ(vsp.command("resume"), "OK");
    EXPECT_TRUE(wait_for(
        [&]() { return sc_core::sc_get_status() == sc_core::SC_STOPPED; }));
}

TEST(vspserver, reconnect) {
    test_vsp_cpu cpu("cpu");
    vspserver session("localhost", 0);

    thread client([&]() {
        run_client(session, cpu);
        cpu.done = true;
    });

    session.start();
    client.join();
}
