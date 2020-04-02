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

#include "testing.h"

TEST(endian, swap) {
    EXPECT_EQ(vcml::swap< u8>(0x11), 0x11);
    EXPECT_EQ(vcml::swap<u16>(0x1122), 0x2211);
    EXPECT_EQ(vcml::swap<u32>(0x11223344), 0x44332211);
    EXPECT_EQ(vcml::swap<u64>(0x1122334455667788ull), 0x8877665544332211ull);
}

TEST(endian, memswap) {
    vcml::u8 x8 = 0x11;
    vcml::memswap(&x8, sizeof(x8));
    EXPECT_EQ(x8, 0x11);

    vcml::u16 x16 = 0x1122;
    vcml::memswap(&x16, sizeof(x16));
    EXPECT_EQ(x16, 0x2211);

    vcml::u32 x32 = 0x11223344;
    vcml::memswap(&x32, sizeof(x32));
    EXPECT_EQ(x32, 0x44332211);

    vcml::u64 x64 = 0x1122334455667788ull;
    vcml::memswap(&x64, sizeof(x64));
    EXPECT_EQ(x64, 0x8877665544332211ull);
}
