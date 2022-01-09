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

#ifndef VCML_UI_FBMODE_H
#define VCML_UI_FBMODE_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"

namespace vcml { namespace ui {

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

    struct color_channel {
        u8 offset;
        u8 size;
    };

    struct fbmode {
        u32 resx;
        u32 resy;

        size_t bpp;
        size_t stride;
        size_t size;

        pixelformat format;

        color_channel a;
        color_channel r;
        color_channel g;
        color_channel b;

        endianess endian;

        bool is_valid() const { return size > 0; }

        bool operator == (const fbmode& other) const;
        bool operator != (const fbmode& other) const;

        string to_string() const;

        static fbmode a8r8g8b8(u32 width, u32 height);
        static fbmode x8r8g8b8(u32 width, u32 height);
        static fbmode r8g8b8a8(u32 width, u32 height);
        static fbmode r8g8b8x8(u32 width, u32 height);
        static fbmode a8b8g8r8(u32 width, u32 height);
        static fbmode x8b8g8r8(u32 width, u32 height);
        static fbmode b8g8r8a8(u32 width, u32 height);
        static fbmode b8g8r8x8(u32 width, u32 height);

        static fbmode r8g8b8(u32 width, u32 height);
        static fbmode b8g8r8(u32 width, u32 height);

        static fbmode r5g6b5(u32 width, u32 height);
        static fbmode b5g6r5(u32 width, u32 height);

        static fbmode gray8(u32 width, u32 height);
    };

    inline bool fbmode::operator == (const fbmode& other) const {
        return resx == other.resx &&  resy == other.resy &&
               format == other.format && endian == other.endian;
    }

    inline bool fbmode::operator != (const fbmode& other) const {
        return !operator == (other);
    }

    ostream& operator << (ostream& os, const fbmode& mode);

}}

#endif
