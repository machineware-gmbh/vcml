/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(clk, to_string) {
    clk_payload tx;
    tx.oldhz = 0;
    tx.newhz = 10;

    stringstream ss1;
    ss1 << tx;
    EXPECT_EQ(ss1.str(), "CLK [off->10Hz]");

    tx.oldhz = 10;
    tx.newhz = 0;

    stringstream ss2;
    ss2 << tx;
    EXPECT_EQ(ss2.str(), "CLK [10Hz->off]");
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
    return arg.oldhz == (vcml::hz_t)oldhz && arg.newhz == (vcml::hz_t)newhz;
}

class clk_bench : public test_base
{
public:
    clk_initiator_socket clk_out;
    clk_base_initiator_socket clk_out_h;
    clk_base_target_socket clk_in_h;
    clk_target_socket clk_in;

    clk_initiator_array clk_array_out;
    clk_target_array clk_array_in;

    clk_bench(const sc_module_name& nm):
        test_base(nm),
        clk_out("clk_out"),
        clk_out_h("clk_out_h"),
        clk_in_h("clk_in_h"),
        clk_in("clk_in"),
        clk_array_out("clk_array_out"),
        clk_array_in("clk_array_in") {
        EXPECT_FALSE(clk_out.is_bound());
        EXPECT_FALSE(clk_out_h.is_bound());
        EXPECT_FALSE(clk_in_h.is_bound());
        EXPECT_FALSE(clk_in.is_bound());
        clk_bind(*this, "clk_out", *this, "clk_out_h");
        clk_bind(*this, "clk_in_h", *this, "clk_in");
        clk_bind(*this, "clk_out_h", *this, "clk_in_h");
        EXPECT_TRUE(clk_out.is_bound());
        EXPECT_TRUE(clk_out_h.is_bound());
        EXPECT_TRUE(clk_in_h.is_bound());
        EXPECT_TRUE(clk_in.is_bound());
        EXPECT_FALSE(clk_array_out[5].is_stubbed());
        EXPECT_FALSE(clk_array_in[6].is_stubbed());
        clk_stub(*this, "clk_array_out", 5, 0 * MHz);
        clk_target(*this, "clk_array_in", 6).stub(0);
        EXPECT_TRUE(clk_array_out[5].is_stubbed());
        EXPECT_TRUE(clk_array_in[6].is_stubbed());

        // test binding multiple targets to one initiator
        clk_bind(*this, "clk_out", *this, "clk_array_in", 6);

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
        EXPECT_EQ(clk_out.cycle(), SC_ZERO_TIME);

        // Turn on clock, make sure events trigger
        EXPECT_CALL(*this, clk_notify(clk_match_socket("clk_in"),
                                      clk_match_payload(0, 100 * MHz)));
        EXPECT_CALL(*this, clk_notify(clk_match_socket("clk_array_in[6]"),
                                      clk_match_payload(0, 100 * MHz)));
        clk_out = 100 * MHz;
        EXPECT_EQ(clk_out, 100 * MHz) << "clk port did not update";
        EXPECT_EQ(clk_out.cycle(), sc_time(10, SC_NS)) << "wrong cycle";
        EXPECT_EQ(clk_out.cycles(2), sc_time(20, SC_NS)) << "wrong cycles";

        // Setting same frequency should not trigger anything
        EXPECT_CALL(*this, clk_notify(_, _)).Times(0);
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
