/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "vcml/ui/fbmode.h"

namespace vcml { namespace ui {

    const char* pixelformat_to_str(pixelformat fmt) {
        switch (fmt) {
        case FORMAT_A8R8G8B8: return "a8r8g8b8";
        case FORMAT_X8R8G8B8: return "x8r8g8b8";
        case FORMAT_R8G8B8A8: return "r8g8b8a8";
        case FORMAT_R8G8B8X8: return "r8g8b8x8";
        case FORMAT_A8B8G8R8: return "a8b8g8r8";
        case FORMAT_X8B8G8R8: return "x8b8g8r8";
        case FORMAT_B8G8R8A8: return "b8g8r8a8";
        case FORMAT_B8G8R8X8: return "b8g8r8x8";
        case FORMAT_R8G8B8:   return "r8g8b8";
        case FORMAT_B8G8R8:   return "b8g8r8";
        case FORMAT_R5G6B5:   return "r5g6b5";
        case FORMAT_B5G6R5:   return "b5g6r5";
        case FORMAT_GRAY8:    return "gray8";
        default:
            return "unknown";
        }
    }

    fbmode fbmode::a8r8g8b8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_A8R8G8B8;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset = 24;
        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::x8r8g8b8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_X8R8G8B8;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::r8g8b8a8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_R8G8B8A8;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset = 24;
        mode.g.offset = 16;
        mode.b.offset =  8;
        mode.a.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::r8g8b8x8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_X8R8G8B8;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset = 24;
        mode.g.offset = 16;
        mode.b.offset =  8;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::a8b8g8r8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_A8B8G8R8;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset = 24;
        mode.b.offset = 16;
        mode.g.offset =  8;
        mode.r.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::x8b8g8r8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_B8G8R8X8;

        mode.a.size = 0;
        mode.a.offset =  0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.b.offset = 16;
        mode.g.offset =  8;
        mode.r.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::b8g8r8a8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_B8G8R8A8;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset =  0;
        mode.r.offset =  8;
        mode.g.offset = 16;
        mode.b.offset = 24;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::b8g8r8x8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.format = FORMAT_B8G8R8X8;

        mode.a.size = 0;
        mode.a.offset =  0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset =  8;
        mode.g.offset = 16;
        mode.b.offset = 24;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::r8g8b8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 3ul * width * height;

        mode.format = FORMAT_R8G8B8;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::b8g8r8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 3ul * width * height;

        mode.format = FORMAT_B8G8R8;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset =  0;
        mode.g.offset =  8;
        mode.b.offset = 16;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::r5g6b5(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 2ul * width * height;

        mode.format = FORMAT_R5G6B5;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = 5;
        mode.g.size = 6;
        mode.b.size = 5;

        mode.r.offset = 11;
        mode.g.offset = 5;
        mode.b.offset = 0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::b5g6r5(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 2ul * width * height;

        mode.format = FORMAT_R5G6B5;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = 5;
        mode.g.size = 6;
        mode.b.size = 5;

        mode.r.offset = 0;
        mode.g.offset = 5;
        mode.b.offset = 11;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode::gray8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 1ul * width * height;

        mode.format = FORMAT_GRAY8;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = 8;
        mode.g.size = 8;
        mode.b.size = 8;

        mode.r.offset = 0;
        mode.g.offset = 0;
        mode.b.offset = 0;

        mode.endian = host_endian();

        return mode;
    }

}}
