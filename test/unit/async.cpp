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

        EXPECT_FALSE(is_sysc_thread());

        while (t < duration) {
            mwr::usleep(1000);
            t += step;
            sc_progress(step);
        }

        sc_sync([&]() -> void {
            EXPECT_TRUE(is_sysc_thread());
            wait(duration);
            success = true;
        });
    }

    void test_async() {
        EXPECT_FALSE(success);
        EXPECT_TRUE(is_sysc_thread());
        EXPECT_FALSE(sc_is_async());
        EXPECT_EQ(sc_time_stamp(), SC_ZERO_TIME);

        sc_time dura(10, SC_SEC);
        auto fn = [&]() -> void { work(dura); };
        sc_async(fn);
        sc_join_async();

        EXPECT_TRUE(success);
        EXPECT_EQ(sc_time_stamp(), 2 * dura);
    }

    void test_suspend() {
        EXPECT_TRUE(vcml::is_sysc_thread());
        EXPECT_FALSE(vcml::sc_is_async());

        atomic<bool> running = false;
        atomic<unsigned int> cnt = 0;

        thread t([&]() -> void {
            EXPECT_FALSE(vcml::is_sysc_thread());
            EXPECT_FALSE(vcml::sc_is_async());

            // wait for async thread to start
            while (!running)
                mwr::cpu_yield();

            // let the async thread run for a while
            mwr::usleep(100);
            EXPECT_GT(cnt, 0);

            // suspend the async thread and check that it does not progress
            vcml::sc_suspend_async(this);
            unsigned int tmp_cnt = cnt;
            mwr::usleep(100);
            EXPECT_EQ(cnt, tmp_cnt);
            tmp_cnt = cnt;

            // resume the async thread and check that it progresses again
            vcml::sc_resume_async(this);
            mwr::usleep(100);
            EXPECT_GT(cnt, tmp_cnt);

            // stop the async thread
            running = false;
        });

        sc_async([&]() -> void {
            EXPECT_FALSE(vcml::is_sysc_thread());
            EXPECT_TRUE(vcml::sc_is_async());
            running = true;

            while (running) {
                cnt++;
                mwr::usleep(10);
                sc_progress(sc_time(1, SC_SEC));
            }
        });

        EXPECT_TRUE(t.joinable());
        t.join();
        sc_join_async();
    }

    virtual void run_test() override {
        test_async();
        test_suspend();
    }
};

TEST(async, run) {
    async_test test("async");
    sc_core::sc_start();
}
