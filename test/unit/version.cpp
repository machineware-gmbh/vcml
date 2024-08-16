/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <iostream>
#include <cstring>
#include <gtest/gtest.h>
#include "vcml.h"

#ifndef VCML_VERSION
#error VCML_VERSION undefined
#endif

#ifndef VCML_VERSION_STRING
#error VCML_VERSION_STRING undefined
#endif

TEST(version, vstr) {
    std::cout << VCML_VERSION << std::endl;
    std::cout << VCML_VERSION_STRING << std::endl;

    EXPECT_GT(VCML_VERSION, 20200000);
    EXPECT_GT(std::strlen(VCML_VERSION_STRING), 0);
    EXPECT_GE(std::strlen(VCML_GIT_REV), std::strlen(VCML_GIT_REV_SHORT));

    EXPECT_NE(mwr::modules::find("vcml"), nullptr);
    EXPECT_NE(mwr::modules::find("systemc"), nullptr);
}
