/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

using namespace ::vcml;

TEST(sc_time, max_time) {
    EXPECT_EQ(sc_max_time().value(), ~0ull);
}

TEST(sc_time, time_unit_is_resolvable) {
    EXPECT_TRUE(time_unit_is_resolvable(SC_SEC));
    EXPECT_TRUE(time_unit_is_resolvable(SC_MS));
    EXPECT_TRUE(time_unit_is_resolvable(SC_US));
    EXPECT_TRUE(time_unit_is_resolvable(SC_NS));
    EXPECT_TRUE(time_unit_is_resolvable(SC_PS));
    EXPECT_FALSE(time_unit_is_resolvable(SC_FS));
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
    EXPECT_FALSE(time_unit_is_resolvable(SC_AS));
    EXPECT_FALSE(time_unit_is_resolvable(SC_ZS));
    EXPECT_FALSE(time_unit_is_resolvable(SC_YS));
#endif
}

TEST(sc_time, to_string) {
    EXPECT_EQ(mwr::to_string(SC_ZERO_TIME), "0s");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_SEC)), "42s");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_MS)), "42ms");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_US)), "42us");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_NS)), "42ns");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_PS)), "42ps");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_FS)), "0s");
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_AS)), "0s");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_ZS)), "0s");
    EXPECT_EQ(mwr::to_string(sc_time(42.0, SC_YS)), "0s");
#endif

    EXPECT_EQ(mwr::to_string(sc_time(1000.0, SC_US)), "1ms");
    EXPECT_EQ(mwr::to_string(sc_time(1001.0, SC_US)), "1001us");

    EXPECT_EQ(mwr::to_string(sc_time(1000.0, SC_FS)), "1ps");
    EXPECT_EQ(mwr::to_string(sc_time(500.0, SC_FS)), "1ps");
    EXPECT_EQ(mwr::to_string(sc_time(499.0, SC_FS)), "0s");
}

int sc_main(int argc, char** argv) {
    ADD_FAILURE() << "sc_main should not be called";
    return EXIT_FAILURE;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::mwr::report_segfaults();

    sc_set_time_resolution(1, SC_PS);

    return RUN_ALL_TESTS();
}
