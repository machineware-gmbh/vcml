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

    extern const u8 crc7_table[256]; // x^7 + x^3 + 1

    inline u8 crc7(const u8* buffer, size_t len) {
        u8 crc = 0;
        while (len--)
            crc = crc7_table[crc ^ *buffer++];
        return crc;
    }

    extern const u16 crc16_table[256]; // x^16 + x^12 + x^5 + 1

    inline u16 crc16(const u8* buffer, size_t len) {
        u16 crc = 0;
        while (len--)
            crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *buffer++) & 0xff];
        return crc;
    }
}

#endif
