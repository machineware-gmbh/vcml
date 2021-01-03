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

#include <iostream>
#include <cstring>
#include <gtest/gtest.h>
#include "vcml.h"

using namespace vcml;
using namespace vcml::ui;

TEST(vnc, fbmode) {
    u32 resx = 800;
    u32 resy = 600;
    vnc_fbmode mode;

    mode = fbmode_argb32(resx, resy);
    EXPECT_EQ(mode.resx, resx);
    EXPECT_EQ(mode.resy, resy);
    EXPECT_EQ(mode.size, resx * resy * 4);

    mode = fbmode_bgra32(resx, resy);
    EXPECT_EQ(mode.resx, resx);
    EXPECT_EQ(mode.resy, resy);
    EXPECT_EQ(mode.size, resx * resy * 4);

    mode = fbmode_rgb24(resx, resy);
    EXPECT_EQ(mode.resx, resx);
    EXPECT_EQ(mode.resy, resy);
    EXPECT_EQ(mode.size, resx * resy * 3);

    mode = fbmode_bgr24(resx, resy);
    EXPECT_EQ(mode.resx, resx);
    EXPECT_EQ(mode.resy, resy);
    EXPECT_EQ(mode.size, resx * resy * 3);

    mode = fbmode_rgb16(resx, resy);
    EXPECT_EQ(mode.resx, resx);
    EXPECT_EQ(mode.resy, resy);
    EXPECT_EQ(mode.size, resx * resy * 2);

    mode = fbmode_gray8(resx, resy);
    EXPECT_EQ(mode.resx, resx);
    EXPECT_EQ(mode.resy, resy);
    EXPECT_EQ(mode.size, resx * resy * 1);
}

TEST(vnc, server) {
    u16 port1 = 40000;
    u16 port2 = 40001;

    shared_ptr<vnc> p1 = vnc::lookup(port1);
    shared_ptr<vnc> p2 = vnc::lookup(port1);
    shared_ptr<vnc> p3 = vnc::lookup(port2);
    shared_ptr<vnc> p4 = vnc::lookup(port2);
    shared_ptr<vnc> p5 = vnc::lookup(port2);

    EXPECT_EQ(p1->port(), port1);
    EXPECT_EQ(p2->port(), port1);
    EXPECT_EQ(p3->port(), port2);
    EXPECT_EQ(p4->port(), port2);
    EXPECT_EQ(p5->port(), port2);

    EXPECT_EQ(p1, p2);
    EXPECT_EQ(p3, p4);
    EXPECT_EQ(p4, p5);

    EXPECT_NE(p1, p3);
    EXPECT_NE(p1, p4);
    EXPECT_NE(p1, p5);

    EXPECT_NE(p2, p3);
    EXPECT_NE(p2, p4);
    EXPECT_NE(p2, p5);

    EXPECT_EQ(p1.use_count(), 3);
    EXPECT_EQ(p2.use_count(), 3);
    EXPECT_EQ(p3.use_count(), 4);
    EXPECT_EQ(p4.use_count(), 4);
    EXPECT_EQ(p5.use_count(), 4);

    p1->shutdown();
    p2->shutdown();
    p3->shutdown();
    p4->shutdown();
    p5->shutdown();
}
