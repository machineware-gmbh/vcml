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

namespace vcml { namespace ui {

    typedef struct {
        u8 offset;
        u8 size;
    } color_format;

    typedef struct {
        u32 resx;
        u32 resy;
        u64 size;
        color_format a;
        color_format r;
        color_format g;
        color_format b;
        vcml_endian endian;
    } fbmode;

    fbmode fbmode_argb32(u32 width, u32 height);
    fbmode fbmode_bgra32(u32 width, u32 height);
    fbmode fbmode_rgb24(u32 width, u32 height);
    fbmode fbmode_bgr24(u32 width, u32 height);
    fbmode fbmode_rgb16(u32 width, u32 height);
    fbmode fbmode_gray8(u32 width, u32 height);

}}

#endif
