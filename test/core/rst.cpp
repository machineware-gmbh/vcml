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

TEST(rst, to_string) {
    rst_payload rst;
    rst.signal = RST_PULSE;
    rst.reset  = true;

    stringstream ss;
    ss << rst;
    EXPECT_EQ(ss.str(), "RST^");
}

TEST(rst, result) {
    rst_payload rst;
    rst.signal = RST_PULSE;
    rst.reset  = true;

    EXPECT_TRUE(success(rst));
    EXPECT_FALSE(failed(rst));
}

MATCHER_P(rst_match_socket, name, "Matches an rst socket by name") {
    return strcmp(arg.basename(), name) == 0;
}

MATCHER_P2(rst_match_payload, sig, val, "Matches an rst payload") {
    return arg.signal == sig && arg.reset == val;
}

class rst_bench : public test_base
{
public:
    rst_initiator_socket rst_out;
    rst_base_initiator_socket rst_out_h;
    rst_base_target_socket rst_in_h;
    rst_target_socket rst_in;

    rst_initiator_socket_array<> rst_array_out;
    rst_target_socket_array<> rst_array_in;

    rst_bench(const sc_module_name& nm):
        test_base(nm),
        rst_out("rst_out"),
        rst_out_h("rst_out_h"),
        rst_in_h("rst_in_h"),
        rst_in("rst_in"),
        rst_array_out("rst_array_out"),
        rst_array_in("rst_array_in") {
        EXPECT_FALSE(rst_out.is_bound());
        EXPECT_FALSE(rst_out_h.is_bound());
        EXPECT_FALSE(rst_in_h.is_bound());
        EXPECT_FALSE(rst_in.is_bound());
        rst_out.bind(rst_out_h);
        rst_in_h.bind(rst_in);
        rst_out_h.bind(rst_in_h);
        EXPECT_TRUE(rst_out.is_bound());
        EXPECT_TRUE(rst_out_h.is_bound());
        EXPECT_TRUE(rst_in_h.is_bound());
        EXPECT_TRUE(rst_in.is_bound());
        EXPECT_FALSE(rst_array_out[5].is_stubbed());
        EXPECT_FALSE(rst_array_in[6].is_stubbed());
        rst_array_out[5].stub();
        rst_array_in[6].stub();
        EXPECT_TRUE(rst_array_out[5].is_stubbed());
        EXPECT_TRUE(rst_array_in[6].is_stubbed());

        // test binding multiple targets to one initiator
        rst_out.bind(rst_array_in[2]);

        // did the port get created?
        EXPECT_TRUE(find_object("rst.rst_array_out[5]"));
        EXPECT_TRUE(find_object("rst.rst_array_in[6]"));

        // did the stubs get created?
        EXPECT_TRUE(find_object("rst.rst_array_out[5]_stub"));
        EXPECT_TRUE(find_object("rst.rst_array_in[6]_stub"));

        // do we have default events?
        EXPECT_STREQ(rst_in.default_event().name(), "rst.rst_in_ev");
    }

    MOCK_METHOD(void, rst_notify,
                (const rst_target_socket&, const rst_payload&));

    virtual void run_test() override {
        // Test pulse resets
        EXPECT_CALL(*this, rst_notify(rst_match_socket("rst_in"),
                                      rst_match_payload(RST_PULSE, true)));
        EXPECT_CALL(*this, rst_notify(rst_match_socket("rst_array_in[2]"),
                                      rst_match_payload(RST_PULSE, true)));
        rst_out.reset();
        EXPECT_FALSE(rst_out) << "port state should not change during pulse";

        // Test noop-pulses
        EXPECT_CALL(*this, rst_notify(_, _)).Times(0);
        rst_out.reset(false, RST_PULSE);
        EXPECT_FALSE(rst_out) << "port state should not change during pulse";

        // Test level resets
        EXPECT_CALL(*this, rst_notify(rst_match_socket("rst_in"),
                                      rst_match_payload(RST_LEVEL, true)));
        EXPECT_CALL(*this, rst_notify(rst_match_socket("rst_array_in[2]"),
                                      rst_match_payload(RST_LEVEL, true)));
        rst_out = true;
        wait(rst_in.default_event());
        EXPECT_TRUE(rst_out) << "port did not set reset signal state";

        // Test same level
        EXPECT_CALL(*this, rst_notify(_, _)).Times(0);
        rst_out = true;

        // Test lowering level
        EXPECT_CALL(*this, rst_notify(rst_match_socket("rst_in"),
                                      rst_match_payload(RST_LEVEL, false)));
        EXPECT_CALL(*this, rst_notify(rst_match_socket("rst_array_in[2]"),
                                      rst_match_payload(RST_LEVEL, false)));
        rst_out = false;
        wait(rst_in.default_event());
        EXPECT_FALSE(rst_out) << "port did not clear reset signal state";
    }
};

TEST(rst, simulate) {
    rst_bench bench("rst");
    sc_core::sc_start();
}
