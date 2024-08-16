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
#include "vcml/core/systemc.h"

class async_test : public test_base
{
public:
    bool success;

    async_test(const sc_module_name& nm): test_base(nm), success(false) {}

    void work(const sc_time& duration) {
        sc_time t = SC_ZERO_TIME;
        sc_time step = duration / 10;

        EXPECT_FALSE(thctl_is_sysc_thread());

        while (t < duration) {
            mwr::usleep(1000);
            t += step;
            sc_progress(step);
        }

        sc_sync([&]() -> void {
            EXPECT_TRUE(thctl_is_sysc_thread());
            wait(duration);
            success = true;
        });
    }

    virtual void run_test() override {
        EXPECT_FALSE(success);
        EXPECT_TRUE(thctl_is_sysc_thread());
        EXPECT_EQ(sc_time_stamp(), SC_ZERO_TIME);

        sc_time dura(10, SC_SEC);
        auto fn = [&]() -> void { work(dura); };
        sc_async(fn);
        sc_join_async();

        EXPECT_TRUE(success);
        EXPECT_EQ(sc_time_stamp(), 2 * dura);
    }
};

TEST(async, run) {
    async_test test("async");
    sc_core::sc_start();
}
