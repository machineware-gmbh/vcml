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

    vq_message msg;
    msg.dmi = [](u64 addr, u32 size, vcml_access a) -> u8* {
        return (u8*)addr; // guest addr == host addr for this test
    };

    msg.append((uintptr_t)s1, strlen(s1), false);
    msg.append((uintptr_t)s2, strlen(s2), false);
    msg.append((uintptr_t)s3, strlen(s3), true);

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
