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

class attr_cpu : public test_vsp_cpu
{
public:
    property<u32> scalar;
    property<u32, 3> array;
    property<string> text;

    attr_cpu(const sc_module_name& nm):
        test_vsp_cpu(nm),
        scalar("scalar", 42),
        array("array", 7),
        text("text", "a,b c") {}
};

TEST(vspserver, attributes) {
    attr_cpu cpu("cpu");

    vspserver session("localhost", 0);
    thread client([&]() {
        test_vsp_client vsp(session.port()); // client 0
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));

        EXPECT_EQ(vsp.command("geta"), "E,insufficient arguments 1");
        EXPECT_EQ(vsp.command("geta,cpu.scalar"), "OK,42");
        EXPECT_EQ(vsp.command("geta,cpu.array"), "OK,7 7 7");
        EXPECT_EQ(vsp.command("geta,cpu.text"), "OK,a\\,b c");

        // one value per attribute, in the order of the request
        EXPECT_EQ(vsp.command("geta,cpu.text,cpu.array,cpu.scalar,cpu.array"),
                  "OK,a\\,b c,7 7 7,42,7 7 7");

        EXPECT_EQ(vsp.command("seta,cpu.array,1,2,3"), "OK");
        EXPECT_EQ(vsp.command("geta,cpu.scalar,cpu.array"), "OK,42,1 2 3");

        // fails entirely if any of the attributes does not exist
        EXPECT_EQ(vsp.command("geta,cpu.scalar,cpu.nothing,cpu.array"),
                  "E,attribute 'cpu.nothing' not found");

        vsp.command("quit");
    });

    session.start();
    client.join();
}
