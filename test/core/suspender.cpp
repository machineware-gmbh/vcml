/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

class suspender_test: public test_base, debugging::suspender
{
private:
    void test_resume() {
        done = false;
        std::thread t([&]() -> void {
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

        t.join();
    }

    void test_forced_resume() {
        done = false;
        std::thread t([&]() -> void {
            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);

            suspend();

            EXPECT_TRUE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(),
                      (debugging::suspender*)this);

            done = true;

            debugging::suspender::force_resume();

            EXPECT_FALSE(is_suspending());
            EXPECT_EQ(debugging::suspender::current(), nullptr);
        });

        while (!done)
            wait(1, SC_MS);

        EXPECT_TRUE(done);

        t.join();
    }

public:
    bool done;

    suspender_test(const sc_module_name& nm = "test"):
        test_base(nm),
        debugging::suspender("suspender"),
        done(false) {
    }

    virtual void run_test() override {
        EXPECT_EQ(owner(), this);
        EXPECT_STREQ(suspender::name(), "test.suspender");

        test_resume();
        test_forced_resume();
    }
};

TEST(suspender, suspend) {
    suspender_test test;
    sc_core::sc_start();
}
