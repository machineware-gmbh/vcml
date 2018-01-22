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

TEST(range, includes) {
    vcml::range a(100, 300);
    vcml::range b(200, 300);
    EXPECT_TRUE(a.includes(b));
    EXPECT_FALSE(b.includes(a));
    EXPECT_TRUE(b.inside(a));
    EXPECT_FALSE(a.inside(b));
}

TEST(range, overlap) {
    vcml::range a(100, 200);
    vcml::range b(150, 300);
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_TRUE(b.overlaps(a));

    vcml::range c(400, 500);
    EXPECT_FALSE(c.overlaps(a));
    EXPECT_FALSE(c.overlaps(b));

    vcml::range d(500, 600);
    EXPECT_TRUE(d.overlaps(c));
    EXPECT_FALSE(d.overlaps(a));
}

TEST(range, connect) {
    vcml::range a(100, 199);
    vcml::range b(200, 300);
    EXPECT_TRUE(a.connects(b));
    EXPECT_TRUE(b.connects(a));

    vcml::range c(100, 300);
    EXPECT_FALSE(c.connects(a));
    EXPECT_FALSE(a.connects(c));
    EXPECT_FALSE(c.connects(b));
    EXPECT_FALSE(b.connects(c));
}

TEST(range, intersect) {
    vcml::range a(100, 200);
    vcml::range b(150, 250);
    vcml::range c = a.intersect(b);
    vcml::range d = b.intersect(a);
    EXPECT_EQ(c.start, 150);
    EXPECT_EQ(c.end, 200);
    EXPECT_EQ(c, d);
}

TEST(range, transaction) {
    tlm::tlm_generic_payload tx;
    tx.set_address(100);
    tx.set_data_length(20);
    tx.set_streaming_width(20);

    vcml::range a(tx);
    EXPECT_EQ(a.start, tx.get_address());
    EXPECT_EQ(a.length(), tx.get_streaming_width());

    tx.set_streaming_width(0);

    vcml::range b(tx);
    EXPECT_EQ(b.start, tx.get_address());
    EXPECT_EQ(b.length(), tx.get_data_length());
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

