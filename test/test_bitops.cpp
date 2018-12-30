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
#include "vcml.h"

TEST(bitops, crc7) {
    vcml::u8 b0[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    vcml::u8 b1[] = { 0xff, 0xff, 0xff, 0xff, 0xff };
    vcml::u8 b2[] = { 0x11, 0x22, 0x33, 0x44, 0x55 };

    EXPECT_EQ(vcml::crc7(b0, sizeof(b0)), 0x00 << 1);
    EXPECT_EQ(vcml::crc7(b1, sizeof(b1)), 0x11 << 1);
    EXPECT_EQ(vcml::crc7(b2, sizeof(b2)), 0x08 << 1);

    vcml::u8  cmd0[] = { 0x40, 0x00, 0x00, 0x00, 0x00 };
    vcml::u8 cmd17[] = { 0x51, 0x00, 0x00, 0x00, 0x00 };
    vcml::u8  resp[] = { 0x11, 0x00, 0x00, 0x09, 0x00 };

    EXPECT_EQ(vcml::crc7(cmd0,  sizeof(cmd0)),  0x4a << 1);
    EXPECT_EQ(vcml::crc7(cmd17, sizeof(cmd17)), 0x2a << 1);
    EXPECT_EQ(vcml::crc7(resp,  sizeof(resp)),  0x33 << 1);
}

TEST(bitops, crc16) {
    vcml::u8 b0[512] = { 0xFF };
    memset(b0, 0xff, 512);
    EXPECT_EQ(vcml::crc16(b0, sizeof(b0)), 0x7fa1);
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
