/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include <iostream>
#include <cstring>
#include <gtest/gtest.h>
#include "vcml.h"

using namespace vcml;
using namespace vcml::ui;

TEST(display, videomode) {
    u32 resx = 800;
    u32 resy = 600;
    videomode mode;

    mode = videomode::a8r8g8b8(resx, resy);
    EXPECT_EQ(mode.xres, resx);
    EXPECT_EQ(mode.yres, resy);
    EXPECT_EQ(mode.size, resx * resy * 4);

    mode = videomode::b8g8r8a8(resx, resy);
    EXPECT_EQ(mode.xres, resx);
    EXPECT_EQ(mode.yres, resy);
    EXPECT_EQ(mode.size, resx * resy * 4);

    mode = videomode::r8g8b8(resx, resy);
    EXPECT_EQ(mode.xres, resx);
    EXPECT_EQ(mode.yres, resy);
    EXPECT_EQ(mode.size, resx * resy * 3);

    mode = videomode::b8g8r8(resx, resy);
    EXPECT_EQ(mode.xres, resx);
    EXPECT_EQ(mode.yres, resy);
    EXPECT_EQ(mode.size, resx * resy * 3);

    mode = videomode::r5g6b5(resx, resy);
    EXPECT_EQ(mode.xres, resx);
    EXPECT_EQ(mode.yres, resy);
    EXPECT_EQ(mode.size, resx * resy * 2);

    mode = videomode::gray8(resx, resy);
    EXPECT_EQ(mode.xres, resx);
    EXPECT_EQ(mode.yres, resy);
    EXPECT_EQ(mode.size, resx * resy * 1);
}

TEST(display, server) {
    u16 port1 = 40000;
    u16 port2 = 40001;

    ui::backend* p1 = backend::create("null:40000");
    ui::backend* p2 = backend::create("null:40000");
    ui::backend* p3 = backend::create("null:40001");
    ui::backend* p4 = backend::create("null:40001");
    ui::backend* p5 = backend::create("null:40001");

    EXPECT_EQ(p1->id(), port1);
    EXPECT_EQ(p2->id(), port1);
    EXPECT_EQ(p3->id(), port2);
    EXPECT_EQ(p4->id(), port2);
    EXPECT_EQ(p5->id(), port2);

    ui::backend::destroy(p1);
    ui::backend::destroy(p2);
    ui::backend::destroy(p3);
    ui::backend::destroy(p4);
    ui::backend::destroy(p5);
}
