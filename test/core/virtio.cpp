/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

TEST(virtio, msgcopy) {
    const char* s1 = "abc";
    const char* s2 = "def";
    char* s3 = strdup("abcdefg");

    virtio::vq_message msg;
    msg.in.push_back({0, (u8*)s1, (u32)strlen(s1)});
    msg.in.push_back({0, (u8*)s2, (u32)strlen(s2)});
    msg.out.push_back({0, (u8*)s3, (u32)strlen(s3)});

    size_t n = 0;
    char s4[7] = { 0 };
    n = msg.copy_in(s4, 5, 1);
    EXPECT_EQ(n, 5);
    EXPECT_STREQ(s4, "bcdef");

    n = msg.copy_out("EFG", 3, 4);
    EXPECT_EQ(n, 3);
    EXPECT_STREQ(s3, "abcdEFG");

    free(s3);
}
