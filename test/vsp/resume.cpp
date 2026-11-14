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

// a cpu that is slow to wake up: session_resume runs on the SystemC thread
// after it was resumed, but before it re-enters sc_start
class slow_cpu : public test_vsp_cpu
{
public:
    atomic<bool> slow;

    slow_cpu(const sc_module_name& nm): test_vsp_cpu(nm), slow(false) {}

    virtual void session_resume() override {
        test_vsp_cpu::session_resume();
        if (slow)
            mwr::usleep(200000);
    }
};

static bool is_stopped(test_vsp_client& client) {
    return starts_with(client.command("status"), "OK,stopped");
}

// OK,<runstate>,<time-ns>,<delta>[,...]
static u64 time_ns(test_vsp_client& client) {
    vector<string> fields = split(client.command("status"), ',');
    EXPECT_GE(fields.size(), 4);
    return fields.size() < 3 ? 0 : std::stoull(fields[2]);
}

static void run_client(test_vsp_client& client, slow_cpu& cpu) {
    ASSERT_TRUE(is_stopped(client));

    // a stop that arrives before the SystemC thread re-entered sc_start: its
    // sc_pause is reset by sc_start, the simulation suspends within the
    // unlimited sc_start of the resume instead
    cpu.slow = true;
    EXPECT_EQ(client.command("resume"), "OK");
    EXPECT_EQ(client.command("stop"), "OK");
    cpu.slow = false;
    ASSERT_TRUE(wait_for([&]() { return is_stopped(client); }));

    // a step must still end after its duration, rather than continue the
    // unlimited sc_start of the previous resume
    u64 start = time_ns(client);
    EXPECT_EQ(client.command("resume,1us"), "OK");
    ASSERT_TRUE(wait_for([&]() { return is_stopped(client); }, 2000))
        << "step did not complete";
    EXPECT_EQ(client.command("status").substr(0, 15), "OK,stopped:step");
    EXPECT_EQ(time_ns(client), start + 1000);
}

TEST(vspserver, stop_before_resume_completes) {
#if SYSTEMC_VERSION < SYSTEMC_VERSION_2_3_1a
    // older kernels warn when sc_start returns without a delta cycle, which
    // happens here when a pause ends a run that only suspended in its update
    // phase; newer kernels exempt runs that start with pending updates
    sc_report_handler::set_actions(SC_ID_NO_SC_START_ACTIVITY_, SC_DO_NOTHING);
#endif

    slow_cpu cpu("cpu");
    vspserver session("localhost", 0);

    thread client([&]() {
        test_vsp_client vsp(session.port());
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));
        run_client(vsp, cpu);

        // end the simulation, whether it is still running or not; stay
        // connected, otherwise the server pauses it when its last client
        // leaves; the simulation then lingers until we disconnect
        cpu.done = true;
        vsp.command("resume");
        EXPECT_TRUE(wait_finished(vsp));
    });

    session.start();
    client.join();
}
