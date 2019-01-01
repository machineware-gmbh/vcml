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

#ifndef VCML_BITOPS_H
#define VCML_BITOPS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"

namespace vcml {

    // is_aligned returns true if the address is properly aligned for a data
    // type of the given size. It is assumed that size is only 4, 2 or 1 byte.
    inline bool is_aligned(u32 addr, u32 size) {
        return !(addr & (std::min(size, 4u) - 1));
    }

    // mask32 generates an integer value that can be used to mask all bits from
    // l to r, assuming a word width of w. For example:
    //   mask32( 7,  0, 32) = 0x000000ff
    //   mask32(31, 24, 32) = 0xff000000
    inline u32 mask32(int l, int r) {
        return (~0u << (32 - (l - r + 1))) >> (31 - l);
    }

    // bits32 extracts bits from v starting at bit index l until before bit
    // index r and returns them as an integer value. For example:
    //   bits32(0xab, 7, 4) = 0x0000000a
    inline u32 bits32(u32 v, int l, int r) {
        return (v << (31 - l)) >> (31 - l + r);
    }

    // sign_extend32 performs a sign extension on the value v, thereby
    // interpreting bit i as the sign bit. Bits 31..i will be set to 1 if bit i
    // is one, otherwise they will be set to zero.
    inline u32 sign_extend32(u32 v, int i) {
        // Right shift on signed operands is not defined for C, but gcc
        // happens do to exactly what we need, so we use it here (and
        // feel really bad about it!)
        i32 t = (v << (31 - i));
        return t >> (31 - i);
    }

    // fls32 finds the position of the last bit set to 1 and returns it. If no
    // bit is set (i.e. parameter is 0), it returns zero.
    template <typename T>
    inline unsigned int fls(const T& v) {
        for (unsigned int i = (sizeof(T) * 8); i > 0; i--)
            if ((v >> (i - 1)) & 0x1)
                return i;
        return 0;
    }

    // ispwr2 returns true if the given integer is a power of two.
    template <typename T>
    inline bool ispwr2(const T& v) {
        return (v != 0) && (ffs(v) == fls(v));
    }

    // crc7 calculates a 7 bit CRC of the specified data using the polynomial
    // x^7 + x^3 + 1. It will be stored in the upper 7 bits of the result.
    extern const u8 crc7_table[256]; //
    inline u8 crc7(const u8* buffer, size_t len) {
        u8 crc = 0;
        while (len--)
            crc = crc7_table[crc ^ *buffer++];
        return crc;
    }

    // crc16 calculates a 16 bit CRC of the given data using the polynomial
    // // x^16 + x^12 + x^5 + 1.
    extern const u16 crc16_table[256];
    inline u16 crc16(const u8* buffer, size_t len) {
        u16 crc = 0;
        while (len--)
            crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *buffer++) & 0xff];
        return crc;
    }

}

#endif
