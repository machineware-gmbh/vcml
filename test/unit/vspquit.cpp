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

TEST(vspserver, quit) {
    test_vsp_cpu cpu("cpu");

    {
        vspserver session("localhost", 0);
        thread client([&]() {
            test_vsp_client vsp(session.port()); // client 0
            ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));
            EXPECT_TRUE(starts_with(vsp.command("mkbp,cpu,0x1000"), "OK"));
            EXPECT_EQ(vsp.command("resume"), "OK");

            size_t steps = cpu.steps;
            ASSERT_TRUE(wait_for([&]() { return cpu.steps > steps + 100; }));

            // quit while the cpu keeps iterating over its breakpoints; the
            // simulation only stops at the next update phase, so the client
            // and its breakpoint must not be removed before that
            vsp.command("quit");
        });

        session.start();
        client.join();
    }

    EXPECT_TRUE(cpu.breakpoints().empty());
}
