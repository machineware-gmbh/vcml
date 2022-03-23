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

using namespace vcml::debugging;

TEST(elf_reader, init) {
    elf_reader reader(get_resource_path("elf.elf"));

    EXPECT_TRUE(reader.is_little_endian());
    EXPECT_EQ(reader.entry(), 0x4001a0);
    EXPECT_EQ(reader.machine(), 62);
    EXPECT_EQ(reader.segments().size(), 2);
}

TEST(elf_reader, segments) {
    elf_reader reader(get_resource_path("elf.elf"));

    EXPECT_TRUE(reader.is_little_endian());

    vector<elf_segment> segments = reader.segments();
    EXPECT_EQ(segments.size(), 2);

    EXPECT_EQ(segments[0].size, 0x498);
    EXPECT_TRUE(segments[0].r);
    EXPECT_FALSE(segments[0].w);
    EXPECT_TRUE(segments[0].x);

    EXPECT_EQ(segments[1].size, 0x10);
    EXPECT_TRUE(segments[1].r);
    EXPECT_TRUE(segments[1].w);
    EXPECT_FALSE(segments[1].x);

    vector<u8> seg0(segments[0].size);
    vector<u8> seg1(segments[1].size);

    EXPECT_EQ(reader.read_segment(segments[0], seg0.data()), seg0.size());
    EXPECT_EQ(reader.read_segment(segments[1], seg1.data()), seg1.size());

    u8 code[4] = {
        0x7f, 0x45, 0x4c, 0x46, // ELF header at start of code
    };

    u8 data[12] = {
        0x42, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // long global_b = 0x42
        0x04, 0x0, 0x0, 0x0,                     // int global_a = 4;
    };

    EXPECT_EQ(memcmp(seg0.data(), code, 4), 0);
    EXPECT_EQ(memcmp(seg1.data(), data, 12), 0);
}
