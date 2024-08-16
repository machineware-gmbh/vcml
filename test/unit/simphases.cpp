/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class simphase_tester : public test_base
{
public:
    MOCK_METHOD(void, do_end_of_elaboration, ());
    MOCK_METHOD(void, do_start_of_simulation, ());
    MOCK_METHOD(void, do_end_of_simulation, ());

    MOCK_METHOD(void, do_next_update, ());

    MOCK_METHOD(void, do_each_delta_cycle, ());
    MOCK_METHOD(void, do_each_time_step, ());

    void next_update() {
        ASSERT_TRUE(sc_get_curr_simcontext()->update_phase());
        do_next_update();
    }

    simphase_tester(const sc_module_name& nm): test_base(nm) {
        on_end_of_elaboration([&]() { do_end_of_elaboration(); });
        on_start_of_simulation([&]() { do_start_of_simulation(); });
        on_end_of_simulation([&]() { do_end_of_simulation(); });

        EXPECT_CALL(*this, do_end_of_elaboration()).Times(1);
        EXPECT_CALL(*this, do_start_of_simulation()).Times(1);
        EXPECT_CALL(*this, do_end_of_simulation()).Times(1);
    }

    virtual void run_test() override {
        wait(SC_ZERO_TIME);
        EXPECT_EQ(sc_time_stamp(), SC_ZERO_TIME);

        EXPECT_CALL(*this, do_next_update()).Times(1);
        on_next_update([&]() { next_update(); });
        wait(SC_ZERO_TIME);
        EXPECT_CALL(*this, do_next_update()).Times(0);
        wait(1.0, SC_SEC);

        on_each_delta_cycle([&]() { do_each_delta_cycle(); });
        on_each_time_step([&]() { do_each_time_step(); });

        EXPECT_CALL(*this, do_each_delta_cycle()).Times(2);
        EXPECT_CALL(*this, do_each_time_step()).Times(0);

        wait(SC_ZERO_TIME);
        wait(SC_ZERO_TIME);

        EXPECT_CALL(*this, do_each_delta_cycle()).Times(1);
        EXPECT_CALL(*this, do_each_time_step()).Times(1);

        wait(1.0, SC_SEC);

        // delta callback will be triggered once again before the simulation
        // ends; the time callback will be triggered only for older versions
        EXPECT_CALL(*this, do_each_delta_cycle()).Times(1);
#if SYSTEMC_VERSION <= SYSTEMC_VERSION_2_3_1a
        EXPECT_CALL(*this, do_each_time_step()).Times(1);
#else
        EXPECT_CALL(*this, do_each_time_step()).Times(0);
#endif
    }
};

TEST(simphases, test) {
    simphase_tester test("test");
    sc_core::sc_start();
}
