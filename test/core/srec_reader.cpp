/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

const vector<u8> V1 = { 0x7c, 0x08, 0x02, 0xa6, 0x90, 0x01, 0x00,
                        0x04, 0x94, 0x21, 0xff, 0xf0, 0x7c, 0x6c,
                        0x1b, 0x78, 0x7c, 0x8c, 0x23, 0x78, 0x3c,
                        0x60, 0x00, 0x00, 0x38, 0x63, 0x00, 0x00 };

const vector<u8> V2 = { 0x4b, 0xff, 0xff, 0xe5, 0x39, 0x80, 0x00,
                        0x00, 0x7d, 0x83, 0x63, 0x78, 0x80, 0x01,
                        0x00, 0x14, 0x38, 0x21, 0x00, 0x10, 0x7c,
                        0x08, 0x03, 0xa6, 0x4e, 0x80, 0x00, 0x20 };

const vector<u8> V3 = { 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77,
                        0x6f, 0x72, 0x6c, 0x64, 0x2e, 0x0a, 0x00 };

TEST(srec_reader, load) {
    debugging::srec_reader reader(get_resource_path("sample.srec"));

    EXPECT_EQ(reader.header(), "hello");
    EXPECT_EQ(reader.entry(), 0x10000);
    ASSERT_EQ(reader.records().size(), 3);

    EXPECT_EQ(reader.records()[0].addr, 0x00);
    EXPECT_EQ(reader.records()[1].addr, 0x1c);
    EXPECT_EQ(reader.records()[2].addr, 0x38);

    EXPECT_EQ(reader.records()[0].data, V1);
    EXPECT_EQ(reader.records()[1].data, V2);
    EXPECT_EQ(reader.records()[2].data, V3);
}
