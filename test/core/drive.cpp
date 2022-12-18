/******************************************************************************
 *                                                                            *
 * Copyright 2022 MachineWare GmbH                                            *
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

TEST(drive, ramdisk) {
    log_term log;
    log.set_level(LOG_DEBUG);

    block::drive drive("drive", "ramdisk:8MiB");
    EXPECT_EQ(drive.capacity(), 8 * MiB);
    EXPECT_EQ(drive.pos(), 0);
    EXPECT_EQ(drive.remaining(), drive.capacity());

    vector<u8> a{ 0x12, 0x34, 0x56, 0x78 };
    vector<u8> b{ 0x00, 0x00, 0x00, 0x00 };

    EXPECT_TRUE(drive.seek(0xffe));
    EXPECT_TRUE(drive.write(a));
    EXPECT_TRUE(drive.seek(0xffe));
    EXPECT_TRUE(drive.read(b));

    EXPECT_EQ(a, b);

    EXPECT_FALSE(drive.seek(8 * MiB + 1));
    EXPECT_TRUE(drive.seek(8 * MiB - 1));
    EXPECT_FALSE(drive.write(a));
}

static void create_file(const string& path, size_t size) {
    ofstream of(path.c_str(), std::ios::binary | std::ios::out);
    of.seekp(size - 1);
    of.write("\0", 1);
}

TEST(drive, file) {
    log_term log;
    log.set_level(LOG_DEBUG);

    create_file("my.disk", 8 * MiB);

    block::drive drive("drive", "my.disk");
    EXPECT_EQ(drive.capacity(), 8 * MiB);
    EXPECT_EQ(drive.pos(), 0);
    EXPECT_EQ(drive.remaining(), drive.capacity());

    vector<u8> a{ 0x12, 0x34, 0x56, 0x78 };
    vector<u8> b{ 0x00, 0x00, 0x00, 0x00 };

    EXPECT_TRUE(drive.seek(0xffe));
    EXPECT_TRUE(drive.write(a));
    EXPECT_TRUE(drive.seek(0xffe));
    EXPECT_TRUE(drive.read(b));

    EXPECT_EQ(a, b);

    EXPECT_FALSE(drive.seek(8 * MiB + 1));
    EXPECT_TRUE(drive.seek(8 * MiB - 1));
    EXPECT_FALSE(drive.write(a));

    std::remove("my.disk");
}

TEST(drive, nothing) {
    log_term log;
    log.set_level(LOG_DEBUG);

    block::drive drive("drive", "nothing");
    EXPECT_EQ(drive.capacity(), 0);
    EXPECT_EQ(drive.pos(), 0);
    EXPECT_EQ(drive.remaining(), drive.capacity());
}

TEST(drive, perm_okay) {
    log_term log;
    log.set_level(LOG_DEBUG);

    create_file("readonly", 1 * MiB);
    chmod("readonly", S_IREAD);

    block::drive drive("drive", "readonly", true);
    EXPECT_EQ(drive.capacity(), 1 * MiB);

    std::remove("readonly");
}

TEST(drive, perm_fail) {
    log_term log;
    log.set_level(LOG_DEBUG);

    create_file("readonly", 1 * MiB);
    chmod("readonly", S_IREAD);

    block::drive drive("drive", "readonly", false);
    EXPECT_EQ(drive.capacity(), 0);

    std::remove("readonly");
}
