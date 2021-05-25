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

#include <gtest/gtest.h>
using namespace ::testing;

#include "vcml.h"
using namespace ::vcml;

TEST(thread_pool, run) {
    std::atomic<bool> job1_done(false);
    std::atomic<bool> job2_done(false);
    std::atomic<bool> job3_done(false);

    auto job1 = [&]() { job1_done = true; };
    auto job2 = [&]() { job2_done = true; };
    auto job3 = [&]() { job3_done = true; };

    thread_pool::instance().run(job1);
    thread_pool::instance().run(job2);
    thread_pool::instance().run(job3);

    // test will timeout when these are not executed
    while (!job1_done || !job2_done || !job3_done)
        usleep(100);
}

TEST(thread_pool, spawn) {
    const size_t n = 4;
    std::atomic<u64> active(0);
    auto job = [&]() {
        active++;
        while (active != n)
            usleep(1000);
    };

    for (size_t i = 0; i < n; i++)
        thread_pool::instance().run(job);

    while (thread_pool::instance().jobs())
        usleep(1000);

    EXPECT_EQ(thread_pool::instance().workers(), 4) << "pool did not grow";
}

