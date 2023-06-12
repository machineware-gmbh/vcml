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

class suspender_test : public test_base, debugging::suspender
{
public:
    std::thread t0;
    std::thread t1;
    std::thread t2;

    atomic<bool> done;

    void test_resume() {
        done = false;

        t0 = std::thread([&]() -> void {
            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);

            suspend();

            EXPECT_TRUE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(),
                      (debugging::suspender*)this);

            done = true;

            resume();

            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);
        });

        EXPECT_FALSE(done);

        while (!done)
            wait(1, SC_MS);
    }

    void test_forced_resume() {
        t1 = std::thread([&]() -> void {
            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);

            suspend();

            EXPECT_TRUE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(),
                      (debugging::suspender*)this);

            // schedule an sc_stop on main thread
            debugging::suspender::quit();

            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);
        });

        // cannot leave this loop, except with suspender::quit() from t1!
        while (true)
            wait(1, SC_MS);
    }

    void test_thctl() {
        bool done = false;

        t2 = std::thread([&]() -> void {
            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);

            suspend();

            EXPECT_TRUE(is_suspending());

            thctl_enter_critical();
            done = true;
            thctl_exit_critical();

            resume();

            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);
        });

        EXPECT_FALSE(done);

        while (!done)
            wait(1, SC_MS);
    }

    suspender_test(const sc_module_name& nm = "test"):
        test_base(nm),
        debugging::suspender("suspender"),
        t0(),
        t1(),
        t2(),
        done(false) {}

    virtual ~suspender_test() {
        if (t0.joinable())
            t0.join();
        if (t1.joinable())
            t1.join();
        if (t2.joinable())
            t2.join();
    }

    virtual void run_test() override {
        EXPECT_EQ(owner(), this);
        EXPECT_STREQ(suspender::name(), "test.suspender");

        test_resume();
        test_thctl();
        test_forced_resume();
    }
};

TEST(suspender, suspend) {
    suspender_test test;
    sc_core::sc_start();
}
