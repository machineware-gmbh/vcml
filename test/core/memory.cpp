/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

using namespace vcml;

TEST(memory, alignment) {
    EXPECT_TRUE(is_aligned(0x1000, VCML_ALIGN_4K));
    EXPECT_FALSE(is_aligned(0x1001, VCML_ALIGN_4K));

    tlm_memory mem(8 * KiB, VCML_ALIGN_8M);
    EXPECT_TRUE(is_aligned(mem.data(), VCML_ALIGN_8M));
}

TEST(alignment, output) {
    std::stringstream ss;

    ss << VCML_ALIGN_8K;
    EXPECT_EQ(ss.str(), "8k");
    ss.str("");

    ss << VCML_ALIGN_256M;
    EXPECT_EQ(ss.str(), "256M");
    ss.str("");

    ss << VCML_ALIGN_1G;
    EXPECT_EQ(ss.str(), "1G");
    ss.str("");
}

TEST(alignment, input) {
    std::stringstream ss;
    vcml::alignment a;

    ss.seekg(0);
    ss.str("128k");
    ss >> a;
    EXPECT_EQ(a, VCML_ALIGN_128K);

    ss.seekg(0);
    ss.str("64M");
    ss >> a;
    EXPECT_EQ(a, VCML_ALIGN_64M);

    ss.seekg(0);
    ss.str("1K");
    ss >> a;
    EXPECT_EQ(a, VCML_ALIGN_1K);
}

TEST(memory, readwrite) {
    tlm_memory mem(1);

    u8 data = 0x42;
    EXPECT_OK(mem.write(range(0, 0), &data, false)) << "write failed";
    EXPECT_EQ(mem[0], data) << "data not stored";

    EXPECT_AE(mem.write(range(1, 1), &data, false))
        << "out of bounds write succeeded";

    mem.allow_read_only();
    mem[0] = 0;

    EXPECT_CE(mem.write(range(0, 0), &data, false))
        << "read-only memory permitted write";
    EXPECT_EQ(mem[0], 0) << "read-only memory got overwritten";

    EXPECT_OK(mem.write(range(0, 0), &data, true))
        << "read-only memory denied debug write";
    EXPECT_EQ(mem[0], data) << "debug write has no effect";
}

TEST(memory, move) {
    const size_t size = 4 * KiB;

    tlm_memory orig(size);
    u8* data = orig.data();

    tlm_memory move = std::move(orig);

    EXPECT_EQ(orig.size(), 0) << "size not zero after move";
    EXPECT_EQ(move.size(), size) << "size not copied correctly";

    EXPECT_EQ(orig.data(), nullptr) << "memory pointer not cleared after move";
    EXPECT_EQ(move.data(), data) << "memory pointer not moved";
}
