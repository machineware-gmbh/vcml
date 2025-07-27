/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include "vcml.h"

using namespace vcml;
using namespace vcml::audio;

TEST(audio, format_str) {
    EXPECT_STREQ(audio::format_str(FORMAT_U8), "FORMAT_U8");
    EXPECT_STREQ(audio::format_str(FORMAT_S8), "FORMAT_S8");
    EXPECT_STREQ(audio::format_str(FORMAT_U16LE), "FORMAT_U16LE");
    EXPECT_STREQ(audio::format_str(FORMAT_S16LE), "FORMAT_S16LE");
    EXPECT_STREQ(audio::format_str(FORMAT_U16BE), "FORMAT_U16BE");
    EXPECT_STREQ(audio::format_str(FORMAT_S16BE), "FORMAT_S16BE");
    EXPECT_STREQ(audio::format_str(FORMAT_U32LE), "FORMAT_U32LE");
    EXPECT_STREQ(audio::format_str(FORMAT_S32LE), "FORMAT_S32LE");
    EXPECT_STREQ(audio::format_str(FORMAT_U32BE), "FORMAT_U32BE");
    EXPECT_STREQ(audio::format_str(FORMAT_S32BE), "FORMAT_S32BE");
}

TEST(audio, format_bits) {
    EXPECT_EQ(audio::format_bits(FORMAT_U8), 8);
    EXPECT_EQ(audio::format_bits(FORMAT_S8), 8);
    EXPECT_EQ(audio::format_bits(FORMAT_U16LE), 16);
    EXPECT_EQ(audio::format_bits(FORMAT_S16LE), 16);
    EXPECT_EQ(audio::format_bits(FORMAT_U16BE), 16);
    EXPECT_EQ(audio::format_bits(FORMAT_S16BE), 16);
    EXPECT_EQ(audio::format_bits(FORMAT_U32LE), 32);
    EXPECT_EQ(audio::format_bits(FORMAT_S32LE), 32);
    EXPECT_EQ(audio::format_bits(FORMAT_U32BE), 32);
    EXPECT_EQ(audio::format_bits(FORMAT_S32BE), 32);
}

TEST(audio, format_endian) {
    EXPECT_TRUE(audio::format_is_little_endian(FORMAT_U8));
    EXPECT_TRUE(audio::format_is_little_endian(FORMAT_S8));
    EXPECT_TRUE(audio::format_is_little_endian(FORMAT_U16LE));
    EXPECT_TRUE(audio::format_is_little_endian(FORMAT_S16LE));
    EXPECT_FALSE(audio::format_is_little_endian(FORMAT_U16BE));
    EXPECT_FALSE(audio::format_is_little_endian(FORMAT_S16BE));
    EXPECT_TRUE(audio::format_is_little_endian(FORMAT_U32LE));
    EXPECT_TRUE(audio::format_is_little_endian(FORMAT_S32LE));
    EXPECT_FALSE(audio::format_is_little_endian(FORMAT_U32BE));
    EXPECT_FALSE(audio::format_is_little_endian(FORMAT_S32BE));
    EXPECT_FALSE(audio::format_is_big_endian(FORMAT_U8));
    EXPECT_FALSE(audio::format_is_big_endian(FORMAT_S8));
    EXPECT_FALSE(audio::format_is_big_endian(FORMAT_U16LE));
    EXPECT_FALSE(audio::format_is_big_endian(FORMAT_S16LE));
    EXPECT_TRUE(audio::format_is_big_endian(FORMAT_U16BE));
    EXPECT_TRUE(audio::format_is_big_endian(FORMAT_S16BE));
    EXPECT_FALSE(audio::format_is_big_endian(FORMAT_U32LE));
    EXPECT_FALSE(audio::format_is_big_endian(FORMAT_S32LE));
    EXPECT_TRUE(audio::format_is_big_endian(FORMAT_U32BE));
    EXPECT_TRUE(audio::format_is_big_endian(FORMAT_S32BE));
}

TEST(audio, fill_silence) {
    u32 buf;
    fill_silence(&buf, sizeof(buf), FORMAT_U8);
    EXPECT_EQ(buf, 0x7f7f7f7f);
    fill_silence(&buf, sizeof(buf), FORMAT_S8);
    EXPECT_EQ(buf, 0x00000000);
    fill_silence(&buf, sizeof(buf), FORMAT_U16LE);
    EXPECT_EQ(buf, 0x7fff7fff);
    fill_silence(&buf, sizeof(buf), FORMAT_S16LE);
    EXPECT_EQ(buf, 0x00000000);
    fill_silence(&buf, sizeof(buf), FORMAT_U16BE);
    EXPECT_EQ(buf, 0xff7fff7f);
    fill_silence(&buf, sizeof(buf), FORMAT_S16BE);
    EXPECT_EQ(buf, 0x00000000);
    fill_silence(&buf, sizeof(buf), FORMAT_U32LE);
    EXPECT_EQ(buf, 0x7fffffff);
    fill_silence(&buf, sizeof(buf), FORMAT_S32LE);
    EXPECT_EQ(buf, 0x00000000);
    fill_silence(&buf, sizeof(buf), FORMAT_U32BE);
    EXPECT_EQ(buf, 0xffffff7f);
    fill_silence(&buf, sizeof(buf), FORMAT_S32BE);
    EXPECT_EQ(buf, 0x00000000);
}
