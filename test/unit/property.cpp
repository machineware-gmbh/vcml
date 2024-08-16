/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    vcml::property<vcml::u32, 4> prop_array2;
    vcml::property<std::string, 4> prop_array_string;
    vcml::property<vcml::range> prop_range;
    vcml::property<void> prop_void;
    vcml::property<std::vector<vcml::i32>> prop_vector;
    vcml::property<std::vector<vcml::i32>> prop_vector2;
    vcml::property<std::vector<vcml::i32>> prop_vector3;

    test_component(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        prop_str("prop_str", "abc"),
        prop_u64("prop_u64", 0xffffffffffffffff),
        prop_u32("prop_u32", 0xffffffff),
        prop_u16("prop_u16", 0xffff),
        prop_u8("prop_u8", 0xff),
        prop_i32("prop_i32", -1),
        not_inited("prop_not_inited", "not_inited"),
        prop_array("prop_array", 7),
        prop_array2("prop_array2", 9),
        prop_array_string("prop_array_string", "not_inited"),
        prop_range("prop_range", { 1, 2 }),
        prop_void("prop_void", 4, 2),
        prop_vector("prop_vector", { 1, 2, 3 }),
        prop_vector2("prop_vector2", { 1, 2, 3 }),
        prop_vector3("prop_vector3", { 1, 2, 3 }) {
        // nothing to do
    }

    virtual ~test_component() = default;
};

TEST(property, init) {
    vcml::broker broker("test", true);
    broker.define("test.prop_str", "hello world");
    broker.define("test.prop_u64", "0x123456789abcdef0");
    broker.define("test.prop_u32", "12345678");
    broker.define("test.prop_u16", "12345");
    broker.define("test.prop_u8", "123");
    broker.define("test.prop_i32", "-2");
    broker.define("test.prop_array", "1 2 3 4");
    broker.define("test.prop_array2", { 1, 2, 3, 4 });
    broker.define("test.prop_array_string", "abc def x\\ y zzz");
    broker.define("test.prop_range", "0x10..0x1f");
    broker.define("test.prop_void", "0xaabbccdd 0x11223344");
    broker.define("test.prop_vector", { -1, -2, -3, -4 });
    broker.define("test.prop_vector2", { "1", "${test.prop_vector}", "2" });
    broker.define("test.prop_vector3", std::vector<int>{ 9, 8, 7 });

    test_component test("test");
    EXPECT_TRUE(test.prop_str.is_inited());
    EXPECT_EQ((std::string)test.prop_str, "hello world");
    EXPECT_EQ((std::string)test.prop_str.str(), "hello world");
    EXPECT_STREQ(test.prop_str.c_str(), "hello world");
    EXPECT_EQ(test.prop_str.get_default(), "abc");

    EXPECT_TRUE(test.prop_u64.is_inited());
    EXPECT_EQ(test.prop_u64, 0x123456789ABCDEF0);
    EXPECT_EQ(std::string(test.prop_u64.str()), "1311768467463790320");
    EXPECT_EQ(test.prop_u64.get_default(), 0xffffffffffffffff);

    EXPECT_TRUE(test.prop_u32.is_inited());
    EXPECT_EQ(test.prop_u32, 12345678);
    EXPECT_EQ(std::string(test.prop_u32.str()), "12345678");
    EXPECT_EQ(test.prop_u32.get_default(), 0xffffffff);

    EXPECT_TRUE(test.prop_u16.is_inited());
    EXPECT_EQ(test.prop_u16, 12345);
    EXPECT_EQ(std::string(test.prop_u16.str()), "12345");
    EXPECT_EQ(test.prop_u16.get_default(), 0xffff);

    EXPECT_TRUE(test.prop_u8.is_inited());
    EXPECT_EQ(test.prop_u8, 123);
    EXPECT_EQ(std::string(test.prop_u8.str()), "123");
    EXPECT_EQ(test.prop_u8.get_default(), 0xff);

    EXPECT_TRUE(test.prop_i32.is_inited());
    EXPECT_EQ(test.prop_i32, -2);
    EXPECT_EQ(std::string(test.prop_i32.str()), "-2");
    EXPECT_EQ(test.prop_i32.get_default(), -1);

    EXPECT_EQ((std::string)test.not_inited, std::string("not_inited"));
    EXPECT_EQ((std::string)test.not_inited, test.not_inited.get_default());
    EXPECT_TRUE(test.not_inited.is_default());
    EXPECT_FALSE(test.not_inited.is_inited());

    EXPECT_TRUE(test.prop_array.is_inited());
    EXPECT_EQ(test.prop_array.count(), 4);
    EXPECT_EQ(test.prop_array[0], 1);
    EXPECT_EQ(test.prop_array[1], 2);
    EXPECT_EQ(test.prop_array[2], 3);
    EXPECT_EQ(test.prop_array[3], 4);
    EXPECT_EQ(test.prop_array.get_default(), 7);
    EXPECT_EQ(std::string(test.prop_array.str()), "1 2 3 4");

    EXPECT_TRUE(test.prop_array2.is_inited());
    EXPECT_EQ(test.prop_array2.count(), 4);
    EXPECT_EQ(test.prop_array2[0], 1);
    EXPECT_EQ(test.prop_array2[1], 2);
    EXPECT_EQ(test.prop_array2[2], 3);
    EXPECT_EQ(test.prop_array2[3], 4);
    EXPECT_EQ(test.prop_array2.get_default(), 9);
    EXPECT_EQ(std::string(test.prop_array2.str()), "1 2 3 4");

    EXPECT_TRUE(test.prop_array_string.is_inited());
    EXPECT_EQ(test.prop_array_string.count(), 4);
    EXPECT_EQ(test.prop_array_string[0], "abc");
    EXPECT_EQ(test.prop_array_string[1], "def");
    EXPECT_EQ(test.prop_array_string[2], "x y");
    EXPECT_EQ(test.prop_array_string[3], "zzz");
    EXPECT_EQ(std::string(test.prop_array_string.str()), "abc def x\\ y zzz");

    EXPECT_TRUE(test.prop_range.is_inited());
    EXPECT_EQ(test.prop_range.get(), vcml::range(0x10, 0x1f));
    EXPECT_EQ(test.prop_range.get_default(), vcml::range(1, 2));
    EXPECT_STREQ(test.prop_range.str(), "0x00000010..0x0000001f");
    EXPECT_EQ(test.prop_range.length(), 0x1f - 0x10 + 1);

    test.prop_array_string[3] = "z z";
    EXPECT_EQ(std::string(test.prop_array_string.str()),
              "abc def x\\ y z\\ z");

    EXPECT_EQ(test.prop_void.get(0), 0xaabbccdd);
    EXPECT_EQ(test.prop_void[1], 0x11223344);
    EXPECT_EQ(test.prop_void.size(), 4);
    EXPECT_EQ(test.prop_void.count(), 2);
    EXPECT_TRUE(test.prop_void.is_inited());
    EXPECT_FALSE(test.prop_void.is_default());
    EXPECT_STREQ(test.prop_void.str(), "2864434397 287454020");
    test.prop_void.set(0x44002299, 1);
    EXPECT_EQ(test.prop_void[1], 0x44002299);
    EXPECT_DEATH(test.prop_void[2], "index 2 out of bounds");
    EXPECT_DEATH(test.prop_void.set(0, 4), "index 4 out of bounds");
    EXPECT_DEATH(test.prop_void.set(0x100000000, 0), "value too big");
    EXPECT_STREQ(test.prop_void.str(), "2864434397 1140859545");
    test.prop_void.str("4 5");
    EXPECT_STREQ(test.prop_void.str(), "4 5");

    EXPECT_STREQ(test.prop_vector.type(), "vector<i32>");
    EXPECT_EQ(test.prop_vector.get_default().size(), 3);
    EXPECT_TRUE(test.prop_vector.is_inited());
    EXPECT_FALSE(test.prop_vector.is_default());
    EXPECT_EQ(test.prop_vector.count(), 4);
    EXPECT_EQ(test.prop_vector.size(), sizeof(vcml::i32));
    vcml::i32 cmp = -1;
    for (vcml::i32 val : test.prop_vector)
        EXPECT_EQ(val, cmp--);
    EXPECT_STREQ(test.prop_vector.str(), "-1 -2 -3 -4");
    EXPECT_EQ(test.prop_vector[0], -1);
    EXPECT_EQ(test.prop_vector[1], -2);
    EXPECT_EQ(test.prop_vector[2], -3);
    EXPECT_EQ(test.prop_vector[3], -4);

    EXPECT_TRUE(test.prop_vector2.is_inited());
    EXPECT_EQ(test.prop_vector2.count(), 6);
    EXPECT_EQ(test.prop_vector2[0], 1);
    EXPECT_EQ(test.prop_vector2[1], -1);
    EXPECT_EQ(test.prop_vector2[2], -2);
    EXPECT_EQ(test.prop_vector2[3], -3);
    EXPECT_EQ(test.prop_vector2[4], -4);
    EXPECT_EQ(test.prop_vector2[5], 2);
    EXPECT_STREQ(test.prop_vector2.str(), "1 -1 -2 -3 -4 2");

    EXPECT_TRUE(test.prop_vector3.is_inited());
    EXPECT_EQ(test.prop_vector3.count(), 3);
    EXPECT_EQ(test.prop_vector3[0], 9);
    EXPECT_EQ(test.prop_vector3[1], 8);
    EXPECT_EQ(test.prop_vector3[2], 7);
    EXPECT_STREQ(test.prop_vector3.str(), "9 8 7");

    std::stringstream ss;

    ss << test.prop_str;
    EXPECT_EQ(ss.str(), test.prop_str.str());
    ss.str("");

    ss << test.prop_u64;
    EXPECT_EQ(ss.str(), test.prop_u64.str());
    ss.str("");

    ss << test.prop_u32;
    EXPECT_EQ(ss.str(), test.prop_u32.str());
    ss.str("");

    ss << test.prop_u16;
    EXPECT_EQ(ss.str(), test.prop_u16.str());
    ss.str("");

    ss << test.prop_u8;
    EXPECT_EQ(ss.str(), test.prop_u8.str());
    ss.str("");

    ss << test.prop_i32;
    EXPECT_EQ(ss.str(), test.prop_i32.str());
    ss.str("");

    ss << test.prop_array;
    EXPECT_EQ(ss.str(), test.prop_array.str());
    ss.str("");

    ss << test.prop_array_string;
    EXPECT_EQ(ss.str(), test.prop_array_string.str());
    ss.str("");

    ss << test.prop_void;
    EXPECT_EQ(ss.str(), test.prop_void.str());
    ss.str("");

    ss << test.prop_vector;
    EXPECT_EQ(ss.str(), test.prop_vector.str());
    ss.str("");

    EXPECT_EQ(vcml::broker::get_or_default("test.prop_u32", 321), 12345678);
    EXPECT_EQ(vcml::broker::get_or_default("test.prop_u33", 321), 321);
    EXPECT_EQ(vcml::broker::get_or_default<int>("test.prop_u32"), 12345678);
    EXPECT_EQ(vcml::broker::get_or_default<int>("test.prop_u33"), 0);
}
