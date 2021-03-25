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

class async_test: public test_base
{
public:
    bool success;

    async_test(const sc_module_name& nm):
       test_base(nm), success(false) {
    }

    void work(const sc_time& duration) {
        sc_time t = SC_ZERO_TIME;
        sc_time step = duration / 10;

        EXPECT_FALSE(thctl_is_sysc_thread());

        while (t < duration) {
            usleep(1000);
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
        auto fn = [&] () -> void { work(dura); };
        sc_async(fn);

        EXPECT_TRUE(success);
        EXPECT_EQ(sc_time_stamp(), 2 * dura);
    }

};

TEST(async, run) {
    async_test test("async");
    sc_core::sc_start();
}
