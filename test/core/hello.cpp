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

int fortytwo() {
    return 42;
}

TEST(hello, fortytwo) {
    EXPECT_EQ(42, fortytwo());
    EXPECT_EQ(50, fortytwo() + 8);
    EXPECT_GT(99, fortytwo());
}
