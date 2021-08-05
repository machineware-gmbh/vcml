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

class thctl_test: public test_base
{
public:
    atomic<int> crit_count;
    bool crit1_done;
    bool crit2_done;

    thctl_test(const sc_module_name& nm = sc_gen_unique_name("test")):
        test_base(nm),
        crit_count(),
        crit1_done(false),
        crit2_done(false) {
    }

    virtual void run_test() override {
        std::thread t1([&]() -> void {
            ASSERT_FALSE(thctl_is_sysc_thread());
            thctl_guard lock;
            crit_count++;
            usleep(1000);
            crit1_done = true;
            EXPECT_EQ(crit_count, 1);
            crit_count--;
        });

        std::thread t2([&]() -> void {
            ASSERT_FALSE(thctl_is_sysc_thread());
            thctl_guard lock;
            crit_count++;
            usleep(1000);
            crit2_done = true;
            EXPECT_EQ(crit_count, 1);
            crit_count--;
        });

        ASSERT_TRUE(thctl_is_sysc_thread());
        EXPECT_FALSE(crit1_done);
        EXPECT_FALSE(crit2_done);
        EXPECT_EQ(crit_count, 0);

        while (!crit1_done || !crit2_done)
            wait(SC_ZERO_TIME);

        EXPECT_TRUE(crit1_done);
        EXPECT_TRUE(crit2_done);
        EXPECT_EQ(crit_count, 0);

        t1.join();
        t2.join();
    }
};

TEST(thctl, critical) {
    thctl_test test;
    sc_core::sc_start();
}

