/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

    bool handler_called = false;
    vcml::aio_notify(fds[0], [&](int fd, int events)-> bool {
        handler_called = true;
        EXPECT_EQ(fd, fds[0]);

        char buf;
        EXPECT_EQ(read(fd, &buf, 1), 1);
        EXPECT_EQ(buf, msg);
        return false;
    }, vcml::AIO_ONCE);

    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_TRUE(handler_called);

    handler_called = false;

    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_FALSE(handler_called);

    vcml::aio_notify(fds[0], [&](int fd, int events)-> bool {
        handler_called = true;
        EXPECT_EQ(fd, fds[0]);

        char buf;
        EXPECT_EQ(read(fd, &buf, 1), 1);
        EXPECT_EQ(buf, msg);
        return false;
    }, vcml::AIO_ONCE);

    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_TRUE(handler_called);

    int handler_calls = 0;

    vcml::aio_notify(fds[0], [&](int fd, int events)-> bool {
        handler_calls++;
        EXPECT_EQ(fd, fds[0]);

        char buf;
        EXPECT_EQ(read(fd, &buf, 1), 1);
        EXPECT_EQ(buf, msg);
        return false;
    }, vcml::AIO_ALWAYS);

    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_EQ(handler_calls, 3);

    vcml::aio_cancel(fds[0]);
    handler_calls = 0;

    vcml::aio_notify(fds[0], [&](int fd, int events)-> bool {
        handler_calls++;
        EXPECT_EQ(fd, fds[0]);

        char buf;
        EXPECT_EQ(read(fd, &buf, 1), 1);
        EXPECT_EQ(buf, msg);
        return false;
    }, vcml::AIO_ONCE);

    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_EQ(write(fds[1], &msg, 1), 1);
    EXPECT_EQ(handler_calls, 1);

    close(fds[0]);
    close(fds[1]);
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
