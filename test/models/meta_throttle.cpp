/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class throttle_test : public test_base
{
public:
    meta::throttle sleeper;
    meta::throttle waiter;
    meta::throttle unknown;
    meta::throttle disabled;
    debugging::suspender suspender;

    const sc_time interval;
    const u64 interval_us;

    throttle_test(const sc_module_name& nm):
        test_base(nm),
        sleeper("sleeper"),
        waiter("waiter"),
        unknown("unknown"),
        disabled("disabled"),
        suspender("suspender"),
        interval(50, SC_MS),
        interval_us(50000) {
        for (auto* throttle : { &sleeper, &waiter, &unknown }) {
            throttle->update_interval = interval;
            throttle->allow_overrun = false;
        }

        add_test("strings", &throttle_test::test_strings);
        add_test("sleep", &throttle_test::test_sleep);
        add_test("wait", &throttle_test::test_wait);
        add_test("unknown", &throttle_test::test_unknown);
        add_test("overrun", &throttle_test::test_overrun);
        add_test("suspend", &throttle_test::test_suspend);
    }

    void activate(meta::throttle& active) {
        for (auto* throttle : { &sleeper, &waiter, &unknown })
            throttle->rtf = throttle == &active ? 1.0 : 1e9;
        wait(2 * interval);
    }

    void check_throttling(meta::throttle& active) {
        activate(active);

        u64 start = mwr::timestamp_us();
        wait(4 * interval);
        u64 elapsed = mwr::timestamp_us() - start;

        EXPECT_GE(elapsed, 3 * interval_us);
        for (auto* throttle : { &sleeper, &waiter, &unknown, &disabled }) {
            EXPECT_EQ(throttle->is_throttling(), throttle == &active)
                << throttle->name();
        }
    }

    void test_strings() {
        EXPECT_STREQ(sleeper.kind(), "vcml::meta::throttle");
        EXPECT_STREQ(sleeper.version(), VCML_VERSION_STRING);
        EXPECT_EQ(sleeper.method.get(), "sleep");
        EXPECT_EQ(waiter.method.get(), "wait");
        EXPECT_EQ(disabled.rtf, 0.0);
        wait(interval / 2);
    }

    void test_sleep() { check_throttling(sleeper); }

    void test_wait() { check_throttling(waiter); }

    void test_unknown() { check_throttling(unknown); }

    void test_overrun() {
        activate(sleeper);
        EXPECT_TRUE(sleeper.is_throttling());

        sleeper.allow_overrun = true;
        mwr::usleep(3 * interval_us);
        wait(interval);
        EXPECT_FALSE(sleeper.is_throttling());
        wait(interval);
        EXPECT_FALSE(sleeper.is_throttling());

        wait(4 * interval);
        EXPECT_TRUE(sleeper.is_throttling());

        sleeper.allow_overrun = false;
        mwr::usleep(3 * interval_us);
        wait(interval);
        EXPECT_FALSE(sleeper.is_throttling());
        wait(interval);
        EXPECT_TRUE(sleeper.is_throttling());
    }

    void test_suspend() {
        activate(sleeper);
        EXPECT_TRUE(sleeper.is_throttling());

        sleeper.allow_overrun = true;
        atomic<bool> done(false);
        suspender.suspend();
        thread t([&]() {
            while (!suspender.is_suspending())
                mwr::usleep(100);
            mwr::usleep(3 * interval_us);
            suspender.resume();
            done = true;
        });

        while (!done)
            wait(interval);
        t.join();

        size_t throttled = 0;
        for (size_t i = 0; i < 3; i++) {
            wait(interval);
            if (sleeper.is_throttling())
                throttled++;
        }

        EXPECT_GE(throttled, 2);
        sleeper.allow_overrun = false;
    }
};

TEST(meta, throttle) {
    broker broker("test");
    broker.define("test.sleeper.rtf", "1");
    broker.define("test.waiter.rtf", "1");
    broker.define("test.waiter.method", "wait");
    broker.define("test.unknown.rtf", "1");
    broker.define("test.unknown.method", "nothing");

    throttle_test test("test");
    sc_core::sc_start();
}
