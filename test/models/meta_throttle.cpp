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

    u64 suspend_start;
    u64 suspended_us;

    virtual void session_suspend() override {
        test_base::session_suspend();
        suspend_start = mwr::timestamp_us();
    }

    virtual void session_resume() override {
        suspended_us += mwr::timestamp_us() - suspend_start;
        test_base::session_resume();
    }

    throttle_test(const sc_module_name& nm):
        test_base(nm),
        sleeper("sleeper"),
        waiter("waiter"),
        unknown("unknown"),
        disabled("disabled"),
        suspender("suspender"),
        interval(50, SC_MS),
        interval_us(50000),
        suspend_start(0),
        suspended_us(0) {
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

    u64 measure(size_t intervals) {
        u64 start = mwr::timestamp_us();
        wait(intervals * interval);
        return mwr::timestamp_us() - start;
    }

    void check_throttling(meta::throttle& active) {
        activate(active);

        size_t throttled = 0;
        u64 start = mwr::timestamp_us();
        for (size_t i = 0; i < 4; i++) {
            wait(interval);
            if (active.is_throttling())
                throttled++;

            for (auto* throttle : { &sleeper, &waiter, &unknown, &disabled }) {
                if (throttle != &active) {
                    EXPECT_FALSE(throttle->is_throttling())
                        << throttle->name();
                }
            }
        }

        u64 elapsed = mwr::timestamp_us() - start;
        EXPECT_GE(elapsed, 3 * interval_us);
        EXPECT_GT(throttled, 0);
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

        sleeper.allow_overrun = true;
        mwr::usleep(3 * interval_us);
        EXPECT_LT(measure(2), interval_us / 2);
        wait(4 * interval);

        sleeper.allow_overrun = false;
        mwr::usleep(3 * interval_us);
        EXPECT_GE(measure(2), interval_us * 8 / 10);
    }

    void test_suspend() {
        activate(sleeper);

        suspended_us = 0;
        suspender.suspend();
        thread t([&] {
            while (!suspender.is_suspending())
                mwr::usleep(100);
            mwr::usleep(3 * interval_us);
            suspender.resume();
        });

        u64 elapsed = measure(1);
        t.join();

        EXPECT_GE(suspended_us, 3 * interval_us);
        EXPECT_GE(elapsed - suspended_us, interval_us * 8 / 10);
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
