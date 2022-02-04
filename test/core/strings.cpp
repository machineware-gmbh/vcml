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

TEST(strings, mkstr) {
    EXPECT_EQ(vcml::mkstr("%d %s", 42, "fortytwo"), "42 fortytwo");
    EXPECT_EQ(vcml::mkstr("%.9f", 1.987654321), "1.987654321");
}

TEST(strings, split) {
    std::string s = "abc def\nghi\tjkl :.; ";
    std::vector<std::string> v = vcml::split(s, [] (unsigned char c) {
        return isspace(c);
    });

    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.at(0), "abc");
    EXPECT_EQ(v.at(1), "def");
    EXPECT_EQ(v.at(2), "ghi");
    EXPECT_EQ(v.at(3), "jkl");
    EXPECT_EQ(v.at(4), ":.;");
}

TEST(strings, join) {
    std::vector<std::string> v0 = { };
    std::vector<std::string> v1 = {"a"};
    std::vector<std::string> v3 = {"a", "b", "c"};

    EXPECT_EQ(vcml::join(v0, ", "), "");
    EXPECT_EQ(vcml::join(v1, ", "), "a");
    EXPECT_EQ(vcml::join(v3, ", "), "a, b, c");
}

TEST(strings, upper_lower) {
    EXPECT_EQ(vcml::to_upper("true"), "TRUE");
    EXPECT_EQ(vcml::to_upper("TRUE"), "TRUE");
    EXPECT_EQ(vcml::to_lower("true"), "true");
    EXPECT_EQ(vcml::to_lower("TRUE"), "true");
}

TEST(strings, trim) {
    EXPECT_EQ(vcml::trim("\ntest0? \t"), "test0?");
}

TEST(strings, from_string) {
    EXPECT_EQ(vcml::from_string<vcml::u64>("0xF"), 0xf);
    EXPECT_EQ(vcml::from_string<vcml::u64>("0x0000000b"), 0xb);
    EXPECT_EQ(vcml::from_string<vcml::i32>("10"), 10);
    EXPECT_EQ(vcml::from_string<vcml::i32>("-10"), -10);
    EXPECT_EQ(vcml::from_string<vcml::u64>("010"), 8);

    EXPECT_TRUE(vcml::from_string<bool>("true"));
    EXPECT_TRUE(vcml::from_string<bool>("True"));
    EXPECT_TRUE(vcml::from_string<bool>("1"));
    EXPECT_TRUE(vcml::from_string<bool>("0x1234"));
    EXPECT_FALSE(vcml::from_string<bool>("false"));
    EXPECT_FALSE(vcml::from_string<bool>("False"));
    EXPECT_FALSE(vcml::from_string<bool>("0"));
    EXPECT_FALSE(vcml::from_string<bool>("0x0"));
}

TEST(strings, replace) {
    std::string s = "replace this";
    EXPECT_EQ(vcml::replace(s, "this", "done"), 1);
    EXPECT_EQ(s, "replace done");

    std::string s2 = "$dir/file.txt";
    EXPECT_EQ(vcml::replace(s2, "$dir", "/home/user"), 1);
    EXPECT_EQ(s2, "/home/user/file.txt");
}

TEST(strings, contains) {
    std::string s = "hello world";

    EXPECT_TRUE(vcml::contains(s, "hello"));
    EXPECT_TRUE(vcml::contains(s, "o wor"));
    EXPECT_FALSE(vcml::contains(s, "wrold"));

    EXPECT_TRUE(vcml::starts_with(s, "hell"));
    EXPECT_FALSE(vcml::starts_with(s, "world"));

    EXPECT_TRUE(vcml::ends_with(s, "world"));
    EXPECT_FALSE(vcml::ends_with(s, "hello"));
}

