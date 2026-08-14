/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
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

TEST(range, empty_intersect) {
    vcml::range a(300, 400);
    vcml::range b(150, 250);
    vcml::range c = a.intersect(b);
    EXPECT_EQ(c.start, 0);
    EXPECT_EQ(c.end, 0);
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
    vcml::range d = { 30, 50 };
    vcml::range e = { 40, 50 };

    EXPECT_TRUE(a == a);
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(c == a);
    EXPECT_FALSE(c == d);
    EXPECT_FALSE(e == d);

    EXPECT_FALSE(a != a);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(c != a);
    EXPECT_TRUE(c != d);
    EXPECT_TRUE(e != d);

    EXPECT_FALSE(a < a);
    EXPECT_FALSE(a < b);
    EXPECT_TRUE(a < c);
    EXPECT_FALSE(c < a);
    EXPECT_FALSE(c < d);
    EXPECT_FALSE(e < d);

    EXPECT_FALSE(a > a);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a > c);
    EXPECT_TRUE(c > a);
    EXPECT_FALSE(c > d);
    EXPECT_FALSE(e > d);

    a += 5;
    EXPECT_EQ(a, b);

    e -= 10;
    EXPECT_EQ(e, c);

    vcml::range g = a + 15;
    EXPECT_EQ(g, c);

    vcml::range h = c - 15;
    EXPECT_EQ(h, b);
}

TEST(range, tostring) {
    vcml::range a(0x10, 0x20);
    std::string s = to_string(a);
    EXPECT_EQ(s, "0x00000010..0x00000020");

    vcml::range b(0xababababcdcdcdcdull, 0xfefefefe12121212ull);
    std::string t = to_string(b);
    EXPECT_EQ(t, "0xababababcdcdcdcd..0xfefefefe12121212");
}

TEST(range, stream_out) {
    vcml::range a(0x100, 0x1ff);
    std::ostringstream oss0;
    oss0 << a;
    EXPECT_EQ(oss0.str(), "0x00000100..0x000001ff");

    vcml::range b(0x100000000ull, 0x1ffffffffull);
    std::ostringstream oss1;
    oss1 << b;
    EXPECT_EQ(oss1.str(), "0x0000000100000000..0x00000001ffffffff");

    vcml::range c(0xaa, 0xbb);
    std::ostringstream oss2;
    oss2 << std::setw(4) << c;
    EXPECT_EQ(oss2.str(), "0x00aa..0x00bb");

    std::ostringstream oss3;
    oss3 << std::setfill('*') << c;
    EXPECT_EQ(oss3.str(), "0x******aa..0x******bb");
}

TEST(range, stream_in) {
    vcml::range a(0x1000, 0x1fff);
    std::ostringstream oss0;
    oss0 << a;
    std::istringstream iss0(oss0.str());
    vcml::range b;
    iss0 >> b;
    EXPECT_FALSE(iss0.fail());
    EXPECT_EQ(b, a);

    vcml::range c(0xdeadbeef00000000ull, 0xdeadbeefffffffffull);
    std::ostringstream oss1;
    oss1 << c;
    std::istringstream iss1(oss1.str());
    vcml::range d;
    iss1 >> d;
    EXPECT_FALSE(iss1.fail());
    EXPECT_EQ(d, c);

    std::istringstream iss2("not-a-range");
    vcml::range e;
    iss2 >> e;
    EXPECT_TRUE(iss2.fail());
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
