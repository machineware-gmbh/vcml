/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_FBMODE_H
#define VCML_UI_FBMODE_H

#include "vcml/core/types.h"

namespace vcml {
namespace ui {

struct color_channel {
    u8 offset; // offset in bits
    u8 size;   // size in bits
};

enum pixelformat {
    // 32bit color formats
    FORMAT_A8R8G8B8,
    FORMAT_X8R8G8B8,
    FORMAT_R8G8B8A8,
    FORMAT_R8G8B8X8,
    FORMAT_A8B8G8R8,
    FORMAT_X8B8G8R8,
    FORMAT_B8G8R8A8,
    FORMAT_B8G8R8X8,

    // 24bit color formats
    FORMAT_R8G8B8,
    FORMAT_B8G8R8,

    // 16bit color formats
    FORMAT_R5G6B5,
    FORMAT_B5G6R5,

    // 8 bit grayscale format
    FORMAT_GRAY8,
};

const char* pixelformat_to_str(pixelformat fmt);
size_t pixelformat_bpp(pixelformat fmt);
color_channel pixelformat_a(pixelformat fmt);
color_channel pixelformat_r(pixelformat fmt);
color_channel pixelformat_g(pixelformat fmt);
color_channel pixelformat_b(pixelformat fmt);

struct videomode {
    u32 xres;
    u32 yres;

    size_t bpp;
    size_t stride;
    size_t size;

    pixelformat format;

    color_channel a;
    color_channel r;
    color_channel g;
    color_channel b;

    bool grayscale;
    endianess endian;

    videomode();
    videomode(pixelformat fmt, u32 xres, u32 yres);
    videomode(const videomode& other) = default;
    videomode(videomode&& other) = default;
    videomode& operator=(const videomode&) = default;

    bool is_valid() const { return size > 0; }

    bool operator==(const videomode& other) const;
    bool operator!=(const videomode& other) const;

    string to_string() const;

    static videomode a8r8g8b8(u32 width, u32 height);
    static videomode x8r8g8b8(u32 width, u32 height);
    static videomode r8g8b8a8(u32 width, u32 height);
    static videomode r8g8b8x8(u32 width, u32 height);
    static videomode a8b8g8r8(u32 width, u32 height);
    static videomode x8b8g8r8(u32 width, u32 height);
    static videomode b8g8r8a8(u32 width, u32 height);
    static videomode b8g8r8x8(u32 width, u32 height);

    static videomode r8g8b8(u32 width, u32 height);
    static videomode b8g8r8(u32 width, u32 height);

    static videomode r5g6b5(u32 width, u32 height);
    static videomode b5g6r5(u32 width, u32 height);

    static videomode gray8(u32 width, u32 height);
};

inline bool videomode::operator==(const videomode& other) const {
    return xres == other.xres && yres == other.yres &&
           format == other.format && endian == other.endian;
}

inline bool videomode::operator!=(const videomode& other) const {
    return !operator==(other);
}

ostream& operator<<(ostream& os, const videomode& mode);

} // namespace ui
} // namespace vcml

#endif
