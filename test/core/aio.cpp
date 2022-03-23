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

TEST(aio, callback) {
    const char msg = 'X';

    int fds[2];
    EXPECT_EQ(pipe(fds), 0);

    std::mutex mtx;
    mtx.lock();
    std::condition_variable_any cv;
    std::atomic<int> count(0);

    vcml::aio_notify(fds[0], [&](int fd) -> void {
        char buf;
        EXPECT_EQ(fd, fds[0]) << "wrong file descriptor passed to handler";
        EXPECT_EQ(read(fd, &buf, 1), 1) << "cannot read file descriptor";
        EXPECT_EQ(buf, msg) << "read incorrect data";

        count++;
        cv.notify_all();
    });

    EXPECT_EQ(write(fds[1], &msg, 1), 1);

    cv.wait(mtx);
    ASSERT_EQ(count, 1) << "handler called multiple times, should be once";

    vcml::aio_cancel(fds[0]);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_EQ(count, 1) << "handler after being cancelled";

    close(fds[0]);
    close(fds[1]);
}
