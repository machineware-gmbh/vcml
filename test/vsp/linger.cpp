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

class linger_cpu : public test_vsp_cpu
{
public:
    atomic<bool> ended;

    linger_cpu(const sc_module_name& nm): test_vsp_cpu(nm), ended(false) {}

    virtual void end_of_simulation() override {
        ended = true;
        test_vsp_cpu::end_of_simulation();
    }
};

static void run_client(vspserver& session, linger_cpu& cpu) {
    test_vsp_client vsp(session.port()); // client 0
    ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));

    // client 1 stays until the simulation has finished, but its leaving
    // must not end the simulation, since client 0 is still there
    {
        test_vsp_client other(session.port()); // client 1
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 1); }));

        // the cpu calls sc_stop by itself once it is done
        cpu.done = true;
        EXPECT_EQ(vsp.command("resume"), "OK");
        EXPECT_EQ(other.command("resume"), "OK");

        ASSERT_TRUE(wait_finished(vsp));
        ASSERT_TRUE(wait_finished(other));
    }

    ASSERT_TRUE(wait_for([&]() { return !has_client(session, 1); }));
    EXPECT_TRUE(wait_finished(vsp));

    // the simulation lingers in end_of_simulation, but models must not have
    // seen it yet, so that their state is still intact
    EXPECT_EQ(sc_core::sc_get_status(), sc_core::SC_END_OF_SIMULATION);
    EXPECT_FALSE(cpu.ended);

    // inspection still works
    EXPECT_TRUE(starts_with(vsp.command("list"), "OK,<?xml"));
    EXPECT_TRUE(starts_with(vsp.command("geta,cpu.loglvl"), "OK,"));
    EXPECT_TRUE(starts_with(vsp.command("mkbp,cpu,0x1000"), "OK"));

    // but there is nothing left to run
    EXPECT_EQ(vsp.command("resume"), "E,simulation finished");
    EXPECT_EQ(vsp.command("resume,10ns"), "E,simulation finished");
    EXPECT_EQ(vsp.command("step,cpu"), "E,simulation finished");
    EXPECT_EQ(vsp.command("stop"), "OK");
    EXPECT_TRUE(wait_finished(vsp));

    vsp.command("quit");
}

TEST(vspserver, linger) {
    linger_cpu cpu("cpu");

    {
        vspserver session("localhost", 0);
        thread client([&]() { run_client(session, cpu); });
        session.start();
        client.join();
    }

    EXPECT_TRUE(cpu.ended);
    EXPECT_TRUE(cpu.breakpoints().empty());
    EXPECT_EQ(sc_core::sc_get_status(), sc_core::SC_STOPPED);
}
