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
using namespace ::testing;

#include "vcml.h"

class test_component : public vcml::component
{
public:
    vcml::property<std::string> prop_str;
    vcml::property<vcml::u64> prop_u64;
    vcml::property<vcml::u32> prop_u32;
    vcml::property<vcml::u16> prop_u16;
    vcml::property<vcml::u8> prop_u8;
    vcml::property<vcml::i32> prop_i32;
    vcml::property<std::string> not_inited;
    vcml::property<vcml::u32, 4> prop_array;
    vcml::property<std::string, 4> prop_array_string;

    test_component(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        prop_str("prop_str", "abc"),
        prop_u64("prop_u64", 0xFFFFFFFFFFFFFFFF),
        prop_u32("prop_u32", 0xFFFFFFFF),
        prop_u16("prop_u16", 0xFFFF),
        prop_u8("prop_u8", 0xFF),
        prop_i32("prop_i32", -1),
        not_inited("prop_not_inited", "not_inited"),
        prop_array("prop_array", 7),
        prop_array_string("prop_array_string", "not_inited") {}

    virtual ~test_component() { /* nothing to do */
    }
};

TEST(property, init) {
    vcml::broker broker("test");
    broker.define("test.prop_str", "hello world");
    broker.define("test.prop_u64", "0x123456789abcdef0");
    broker.define("test.prop_u32", "12345678");
    broker.define("test.prop_u16", "12345");
    broker.define("test.prop_u8", "123");
    broker.define("test.prop_i32", "-2");
    broker.define("test.prop_array", "1 2 3 4");
    broker.define("test.prop_array_string", "abc def x\\ y zzz");

    test_component test("test");
    EXPECT_EQ((std::string)test.prop_str, "hello world");
    EXPECT_EQ((std::string)test.prop_str.str(), "hello world");
    EXPECT_EQ(test.prop_str.get_default(), "abc");

    EXPECT_EQ(test.prop_u64, 0x123456789ABCDEF0);
    EXPECT_EQ(std::string(test.prop_u64.str()), "1311768467463790320");
    EXPECT_EQ(test.prop_u64.get_default(), 0xFFFFFFFFFFFFFFFF);

    EXPECT_EQ(test.prop_u32, 12345678);
    EXPECT_EQ(std::string(test.prop_u32.str()), "12345678");
    EXPECT_EQ(test.prop_u32.get_default(), 0xFFFFFFFF);

    EXPECT_EQ(test.prop_u16, 12345);
    EXPECT_EQ(std::string(test.prop_u16.str()), "12345");
    EXPECT_EQ(test.prop_u16.get_default(), 0xFFFF);

    EXPECT_EQ(test.prop_u8, 123);
    EXPECT_EQ(std::string(test.prop_u8.str()), "123");
    EXPECT_EQ(test.prop_u8.get_default(), 0xFF);

    EXPECT_EQ(test.prop_i32, -2);
    EXPECT_EQ(std::string(test.prop_i32.str()), "-2");
    EXPECT_EQ(test.prop_i32.get_default(), -1);

    EXPECT_EQ((std::string)test.not_inited, std::string("not_inited"));
    EXPECT_EQ((std::string)test.not_inited, test.not_inited.get_default());

    EXPECT_EQ(test.prop_array.count(), 4);
    EXPECT_EQ(test.prop_array[0], 1);
    EXPECT_EQ(test.prop_array[1], 2);
    EXPECT_EQ(test.prop_array[2], 3);
    EXPECT_EQ(test.prop_array[3], 4);
    EXPECT_EQ(test.prop_array.get_default(), 7);
    EXPECT_EQ(std::string(test.prop_array.str()), "1 2 3 4");

    EXPECT_EQ(test.prop_array_string.count(), 4);
    EXPECT_EQ(test.prop_array_string[0], "abc");
    EXPECT_EQ(test.prop_array_string[1], "def");
    EXPECT_EQ(test.prop_array_string[2], "x y");
    EXPECT_EQ(test.prop_array_string[3], "zzz");
    EXPECT_EQ(std::string(test.prop_array_string.str()), "abc def x\\ y zzz");

    test.prop_array_string[3] = "z z";
    EXPECT_EQ(std::string(test.prop_array_string.str()),
              "abc def x\\ y z\\ z");
}
