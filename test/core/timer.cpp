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

#include <future>
#include "testing.h"

class timer_test : public test_base
{
public:
    timer_test(const sc_module_name& nm): test_base(nm) {}

    virtual void run_test() override {
        timer t1(1, SC_MS, [](timer& t) -> void {
            EXPECT_EQ(sc_time_stamp(), t.timeout());
        });

        timer t2(1, SC_US, [](timer& t) -> void {
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
            timer t3(10, SC_US, [&](timer& t) -> void {
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

TEST(timer, test) {
    timer_test test("timer");
    sc_core::sc_start();
}
