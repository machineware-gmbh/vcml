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


}

std::ostream& operator << (std::ostream& os, vcml::vcml_endian& endian);
std::istream& operator >> (std::istream& is, vcml::vcml_endian& endian);

#endif
