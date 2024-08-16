/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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

TEST(range, init) {
    vcml::range a = { 10, 20 };
    EXPECT_EQ(a.start, 10);
    EXPECT_EQ(a.end, 20);

    vcml::range b({ 20, 30 });
    EXPECT_EQ(b.start, 20);
    EXPECT_EQ(b.end, 30);
}

TEST(range, operators) {
    vcml::range a = { 10, 20 };
    vcml::range b = { 15, 25 };
    vcml::range c = { 30, 40 };

    EXPECT_EQ(a, a);
    EXPECT_NE(a, c);
    EXPECT_LT(a, c);
    EXPECT_GT(c, a);

    EXPECT_FALSE(a > b);
    EXPECT_FALSE(b < a);
}

TEST(range, tostring) {
    vcml::range a = { 0x10, 0x20 };
    std::string s = to_string(a);
    EXPECT_EQ(s, "0x00000010..0x00000020");

    vcml::range b = { 0xababababcdcdcdcdull, 0xfefefefe12121212ull };
    std::string t = to_string(b);
    EXPECT_EQ(t, "0xababababcdcdcdcd..0xfefefefe12121212");
}

TEST(range, limits) {
    vcml::range a(4, 3);
    EXPECT_EQ(a.length(), 0);

    vcml::range b(0, -1ll);
    EXPECT_EQ(a.length(), 0);

    vcml::range c(~0ull - 15, ~0ull);
    EXPECT_EQ(c.length(), 16);

    ASSERT_DEATH(
        { vcml::range d(5, 3); },
        "invalid range: 0000000000000005..0000000000000003");
}
