/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(clk, to_string) {
    clk_payload tx;
    tx.oldhz = 0;
    tx.newhz = 10;

    stringstream ss1;
    ss1 << tx;
    EXPECT_EQ(ss1.str(), "off -> 10Hz");

    tx.oldhz = 10;
    tx.newhz = 0;

    stringstream ss2;
    ss2 << tx;
    EXPECT_EQ(ss2.str(), "10Hz -> off");
}

TEST(clk, result) {
    clk_payload tx;
    tx.oldhz = 0;
    tx.newhz = 0;

    EXPECT_TRUE(success(tx));
    EXPECT_FALSE(failed(tx));
}

MATCHER_P(clk_match_socket, name, "Matches a clk socket by name") {
    return strcmp(arg.basename(), name) == 0;
}

MATCHER_P2(clk_match_payload, oldhz, newhz, "Matches a clk payload") {
    return arg.oldhz == oldhz && arg.newhz == newhz;
}

class clk_bench : public test_base, public clk_host
{
public:
    clk_initiator_socket clk_out;
    clk_base_initiator_socket clk_out_h;
    clk_base_target_socket clk_in_h;
    clk_target_socket clk_in;

    clk_initiator_socket_array<> clk_array_out;
    clk_target_socket_array<> clk_array_in;

    clk_bench(const sc_module_name& nm):
        test_base(nm),
        clk_host(),
        clk_out("clk_out"),
        clk_out_h("clk_out_h"),
        clk_in_h("clk_in_h"),
        clk_in("clk_in"),
        clk_array_out("clk_array_out"),
        clk_array_in("clk_array_in") {
        clk_out.bind(clk_out_h);
        clk_in_h.bind(clk_in);
        clk_out_h.bind(clk_in_h);

        clk_array_out[5].stub();
        clk_array_in[6].stub();

        // test binding multiple targets to one initiator
        clk_out.bind(clk_array_in[6]);

        // did the port get created?
        EXPECT_TRUE(find_object("clk.clk_array_out[5]"));
        EXPECT_TRUE(find_object("clk.clk_array_in[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("clk.clk_array_out[5]_stub"));
        EXPECT_TRUE(find_object("clk.clk_array_in[6]_stub"));
    }

    MOCK_METHOD(void, clk_notify,
                (const clk_target_socket&, const clk_payload&));

    virtual void run_test() override {
        // Make sure clock starts turned off
        EXPECT_EQ(clk_out, 0 * Hz);
        EXPECT_EQ(clk_in, 0 * Hz);
        EXPECT_EQ(clk_array_in[6], 0 * Hz);

        // Turn on clock, make sure events trigger
        EXPECT_CALL(*this, clk_notify(clk_match_socket("clk_in"),
                                      clk_match_payload(0, 100 * MHz)));
        EXPECT_CALL(*this, clk_notify(clk_match_socket("clk_array_in[6]"),
                                      clk_match_payload(0, 100 * MHz)));
        clk_out = 100 * MHz;
        EXPECT_EQ(clk_out, 100 * MHz) << "clk port did not update";

        // Setting same frequency should not trigger anything
        EXPECT_CALL(*this, clk_notify(_,_)).Times(0);
        clk_out = 100 * MHz;
        EXPECT_EQ(clk_out, 100 * MHz) << "clk port changed unexpectedly";

        // Test turning clock off
        EXPECT_CALL(*this, clk_notify(clk_match_socket("clk_in"),
                                      clk_match_payload(100 * MHz, 0)));
        EXPECT_CALL(*this, clk_notify(clk_match_socket("clk_array_in[6]"),
                                      clk_match_payload(100 * MHz, 0)));
        clk_out = 0;
        EXPECT_EQ(clk_out, 0 * Hz) << "clk port did not turn off";
    }
};

TEST(clk, simulate) {
    clk_bench bench("clk");
    sc_core::sc_start();
}
