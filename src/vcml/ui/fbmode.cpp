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

    fbmode fbmode_argb32(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset = 24;
        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode_bgra32(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 4ul * width * height;

        mode.a.size = mode.r.size = mode.g.size = mode.b.size = 8;

        mode.a.offset =  0;
        mode.r.offset =  8;
        mode.g.offset = 16;
        mode.b.offset = 24;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode_rgb24(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 3ul * width * height;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset = 16;
        mode.g.offset =  8;
        mode.b.offset =  0;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode_bgr24(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 3ul * width * height;

        mode.a.size = 0;
        mode.a.offset = 0;

        mode.r.size = mode.g.size = mode.b.size = 8;

        mode.r.offset =  0;
        mode.g.offset =  8;
        mode.b.offset = 16;

        mode.endian = host_endian();

        return mode;
    }

    fbmode fbmode_rgb16(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 2ul * width * height;

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

    fbmode fbmode_gray8(u32 width, u32 height) {
        fbmode mode;

        mode.resx = width;
        mode.resy = height;
        mode.size = 1ul * width * height;

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
