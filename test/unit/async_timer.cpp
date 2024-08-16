/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <future>
#include "testing.h"

class async_timer_test : public test_base
{
public:
    async_timer_test(const sc_module_name& nm): test_base(nm) {}

    virtual void run_test() override {
        async_timer t1(1, SC_MS, [](async_timer& t) -> void {
            EXPECT_EQ(sc_time_stamp(), t.timeout());
        });

        async_timer t2(1, SC_US, [](async_timer& t) -> void {
            EXPECT_EQ(sc_time_stamp(), t.timeout());
            t.reset(1, SC_US);
        });

        EXPECT_EQ(t1.count(), 0);
        EXPECT_EQ(t2.count(), 0);

        wait(1, SC_MS);

        EXPECT_EQ(t1.count(), 1);
        EXPECT_EQ(t2.count(), 1000);

        atomic<bool> running(true);

        std::promise<void> promise;
        std::future<void> future = promise.get_future();

        std::thread async([&]() -> void {
            EXPECT_TRUE(running);
            EXPECT_FALSE(thctl_is_sysc_thread());
            async_timer t3(10, SC_US, [&](async_timer& t) -> void {
                EXPECT_GE(sc_time_stamp(), t.timeout());
                EXPECT_TRUE(thctl_is_sysc_thread());
                running = false;
                promise.set_value();
            });

            future.wait();
        });

        while (running)
            wait(1, SC_US);

        async.join();
    }
};

TEST(async_timer, test) {
    async_timer_test test("test");
    sc_core::sc_start();
}
