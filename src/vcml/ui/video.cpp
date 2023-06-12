/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/video.h"

namespace vcml {
namespace ui {

const char* pixelformat_to_str(pixelformat fmt) {
    switch (fmt) {
    case FORMAT_A8R8G8B8:
        return "a8r8g8b8";
    case FORMAT_X8R8G8B8:
        return "x8r8g8b8";
    case FORMAT_R8G8B8A8:
        return "r8g8b8a8";
    case FORMAT_R8G8B8X8:
        return "r8g8b8x8";
    case FORMAT_A8B8G8R8:
        return "a8b8g8r8";
    case FORMAT_X8B8G8R8:
        return "x8b8g8r8";
    case FORMAT_B8G8R8A8:
        return "b8g8r8a8";
    case FORMAT_B8G8R8X8:
        return "b8g8r8x8";
    case FORMAT_R8G8B8:
        return "r8g8b8";
    case FORMAT_B8G8R8:
        return "b8g8r8";
    case FORMAT_R5G6B5:
        return "r5g6b5";
    case FORMAT_B5G6R5:
        return "b5g6r5";
    case FORMAT_GRAY8:
        return "gray8";
    default:
        return "unknown";
    }
}

size_t pixelformat_bpp(pixelformat fmt) {
    switch (fmt) {
    case FORMAT_A8R8G8B8:
        return 4;
    case FORMAT_X8R8G8B8:
        return 4;
    case FORMAT_R8G8B8A8:
        return 4;
    case FORMAT_R8G8B8X8:
        return 4;
    case FORMAT_A8B8G8R8:
        return 4;
    case FORMAT_X8B8G8R8:
        return 4;
    case FORMAT_B8G8R8A8:
        return 4;
    case FORMAT_B8G8R8X8:
        return 4;
    case FORMAT_R8G8B8:
        return 3;
    case FORMAT_B8G8R8:
        return 3;
    case FORMAT_R5G6B5:
        return 2;
    case FORMAT_B5G6R5:
        return 2;
    case FORMAT_GRAY8:
        return 1;
    default:
        return 0;
    }
}

color_channel pixelformat_a(pixelformat fmt) {
    switch (fmt) {
    case FORMAT_A8R8G8B8:
        return { 24, 8 };
    case FORMAT_X8R8G8B8:
        return { 24, 0 };
    case FORMAT_R8G8B8A8:
        return { 0, 8 };
    case FORMAT_R8G8B8X8:
        return { 0, 0 };
    case FORMAT_A8B8G8R8:
        return { 24, 8 };
    case FORMAT_X8B8G8R8:
        return { 24, 0 };
    case FORMAT_B8G8R8A8:
        return { 0, 8 };
    case FORMAT_B8G8R8X8:
        return { 0, 0 };
    case FORMAT_R8G8B8:
        return { 0, 0 };
    case FORMAT_B8G8R8:
        return { 0, 0 };
    case FORMAT_R5G6B5:
        return { 0, 0 };
    case FORMAT_B5G6R5:
        return { 0, 0 };
    case FORMAT_GRAY8:
        return { 0, 0 };
    default:
        VCML_ERROR("invalid pixelformat: %u", (int)fmt);
    }
}

color_channel pixelformat_r(pixelformat fmt) {
    switch (fmt) {
    case FORMAT_A8R8G8B8:
        return { 16, 8 };
    case FORMAT_X8R8G8B8:
        return { 16, 8 };
    case FORMAT_R8G8B8A8:
        return { 24, 8 };
    case FORMAT_R8G8B8X8:
        return { 24, 8 };
    case FORMAT_A8B8G8R8:
        return { 0, 8 };
    case FORMAT_X8B8G8R8:
        return { 0, 8 };
    case FORMAT_B8G8R8A8:
        return { 8, 8 };
    case FORMAT_B8G8R8X8:
        return { 8, 8 };
    case FORMAT_R8G8B8:
        return { 16, 8 };
    case FORMAT_B8G8R8:
        return { 0, 8 };
    case FORMAT_R5G6B5:
        return { 11, 5 };
    case FORMAT_B5G6R5:
        return { 0, 5 };
    case FORMAT_GRAY8:
        return { 0, 8 };
    default:
        VCML_ERROR("invalid pixelformat: %u", (int)fmt);
    }
}

color_channel pixelformat_g(pixelformat fmt) {
    switch (fmt) {
    case FORMAT_A8R8G8B8:
        return { 8, 8 };
    case FORMAT_X8R8G8B8:
        return { 8, 8 };
    case FORMAT_R8G8B8A8:
        return { 16, 8 };
    case FORMAT_R8G8B8X8:
        return { 16, 8 };
    case FORMAT_A8B8G8R8:
        return { 8, 8 };
    case FORMAT_X8B8G8R8:
        return { 8, 8 };
    case FORMAT_B8G8R8A8:
        return { 16, 8 };
    case FORMAT_B8G8R8X8:
        return { 16, 8 };
    case FORMAT_R8G8B8:
        return { 8, 8 };
    case FORMAT_B8G8R8:
        return { 8, 8 };
    case FORMAT_R5G6B5:
        return { 5, 6 };
    case FORMAT_B5G6R5:
        return { 5, 6 };
    case FORMAT_GRAY8:
        return { 0, 8 };
    default:
        VCML_ERROR("invalid pixelformat: %u", (int)fmt);
    }
}

color_channel pixelformat_b(pixelformat fmt) {
    switch (fmt) {
    case FORMAT_A8R8G8B8:
        return { 0, 8 };
    case FORMAT_X8R8G8B8:
        return { 0, 8 };
    case FORMAT_R8G8B8A8:
        return { 8, 8 };
    case FORMAT_R8G8B8X8:
        return { 8, 8 };
    case FORMAT_A8B8G8R8:
        return { 16, 8 };
    case FORMAT_X8B8G8R8:
        return { 16, 8 };
    case FORMAT_B8G8R8A8:
        return { 24, 8 };
    case FORMAT_B8G8R8X8:
        return { 24, 8 };
    case FORMAT_R8G8B8:
        return { 0, 8 };
    case FORMAT_B8G8R8:
        return { 16, 8 };
    case FORMAT_R5G6B5:
        return { 0, 5 };
    case FORMAT_B5G6R5:
        return { 16, 5 };
    case FORMAT_GRAY8:
        return { 0, 8 };
    default:
        VCML_ERROR("invalid pixelformat: %u", (int)fmt);
    }
}

videomode::videomode():
    xres(),
    yres(),
    bpp(),
    stride(),
    size(),
    format(),
    a(),
    r(),
    g(),
    b(),
    grayscale(),
    endian() {
}

videomode::videomode(pixelformat fmt, u32 w, u32 h):
    xres(w),
    yres(h),
    bpp(pixelformat_bpp(fmt)),
    stride(bpp * xres),
    size(stride * yres),
    format(fmt),
    a(pixelformat_a(fmt)),
    r(pixelformat_r(fmt)),
    g(pixelformat_g(fmt)),
    b(pixelformat_b(fmt)),
    grayscale(fmt == FORMAT_GRAY8),
    endian(ENDIAN_LITTLE) {
}

string videomode::to_string() const {
    const char* fmt = pixelformat_to_str(format);
    const char* end = endian == ENDIAN_BIG ? "BE" : "LE";
    return mkstr("%ux%u %s %s", xres, yres, fmt, end);
}

videomode videomode::a8r8g8b8(u32 width, u32 height) {
    return videomode(FORMAT_A8R8G8B8, width, height);
}

videomode videomode::x8r8g8b8(u32 width, u32 height) {
    return videomode(FORMAT_X8R8G8B8, width, height);
}

videomode videomode::r8g8b8a8(u32 width, u32 height) {
    return videomode(FORMAT_R8G8B8A8, width, height);
}

videomode videomode::r8g8b8x8(u32 width, u32 height) {
    return videomode(FORMAT_R8G8B8X8, width, height);
}

videomode videomode::a8b8g8r8(u32 width, u32 height) {
    return videomode(FORMAT_A8B8G8R8, width, height);
}

videomode videomode::x8b8g8r8(u32 width, u32 height) {
    return videomode(FORMAT_X8B8G8R8, width, height);
}

videomode videomode::b8g8r8a8(u32 width, u32 height) {
    return videomode(FORMAT_B8G8R8A8, width, height);
}

videomode videomode::b8g8r8x8(u32 width, u32 height) {
    return videomode(FORMAT_B8G8R8X8, width, height);
}

videomode videomode::r8g8b8(u32 width, u32 height) {
    return videomode(FORMAT_R8G8B8, width, height);
}

videomode videomode::b8g8r8(u32 width, u32 height) {
    return videomode(FORMAT_B8G8R8, width, height);
}

videomode videomode::r5g6b5(u32 width, u32 height) {
    return videomode(FORMAT_R5G6B5, width, height);
}

videomode videomode::b5g6r5(u32 width, u32 height) {
    return videomode(FORMAT_B5G6R5, width, height);
}

videomode videomode::gray8(u32 width, u32 height) {
    return videomode(FORMAT_GRAY8, width, height);
}

ostream& operator<<(ostream& os, const videomode& mode) {
    return os << mode.to_string();
}

} // namespace ui
} // namespace vcml
