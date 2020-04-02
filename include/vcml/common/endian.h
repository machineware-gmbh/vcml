/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#ifndef VCML_ENDIAN_H
#define VCML_ENDIAN_H

#include "vcml/common/types.h"

namespace vcml {

    enum vcml_endian {
        VCML_ENDIAN_UNKNOWN = 0,
        VCML_ENDIAN_LITTLE = 1,
        VCML_ENDIAN_BIG = 2,
    };

    const char* endian_to_str(int endian);

    inline vcml_endian host_endian() {
        u32 test = 1;
        u8* p = reinterpret_cast<u8*>(&test);
        if (p[0] == 1) return VCML_ENDIAN_LITTLE;
        if (p[3] == 1) return VCML_ENDIAN_BIG;
        return VCML_ENDIAN_UNKNOWN;
    }

    template <typename DATA>
    DATA swap(DATA val);

    template <>
    inline u8 swap<u8>(u8 val) {
        return val;
    }

    template <>
    inline u16 swap<u16>(u16 val) {
        return ((val & 0xff00) >> 8) |
               ((val & 0x00ff) << 8);
    }

    template <>
    inline u32 swap<u32>(u32 val) {
        return ((val & 0xff000000) >> 24) |
               ((val & 0x00ff0000) >>  8) |
               ((val & 0x0000ff00) <<  8) |
               ((val & 0x000000ff) << 24);
    }

    template <>
    inline u64 swap<u64>(u64 val) {
        return ((val & 0xff00000000000000ull) >> 56) |
               ((val & 0x00ff000000000000ull) >> 40) |
               ((val & 0x0000ff0000000000ull) >> 24) |
               ((val & 0x000000ff00000000ull) >>  8) |
               ((val & 0x00000000ff000000ull) <<  8) |
               ((val & 0x0000000000ff0000ull) << 24) |
               ((val & 0x000000000000ff00ull) << 40) |
               ((val & 0x00000000000000ffull) << 56);
    }

    inline void memswap(void* ptr, unsigned int size) {
        u8* v = static_cast<u8*>(ptr);
        for (unsigned int i = 0; i < size / 2; i++)
            std::swap(v[i], v[size - 1 - i]);
    }

}

std::ostream& operator << (std::ostream& os, vcml::vcml_endian& endian);
std::istream& operator >> (std::istream& is, vcml::vcml_endian& endian);

#endif
