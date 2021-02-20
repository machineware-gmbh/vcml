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

#include "vcml/common/types.h"
#include "vcml/common/strings.h"

namespace vcml {

    const char* endian_to_str(int endian) {
        switch (endian) {
        case ENDIAN_LITTLE: return "little";
        case ENDIAN_BIG: return "big";
        default:
            return "unknown";
        }
    }

}

std::istream& operator >> (std::istream& is, vcml::endianess& endian) {
    std::string str;
    is >> str;
    str = vcml::to_lower(str);

    if (str == "big")
        endian = vcml::ENDIAN_BIG;
    else if (str == "little")
        endian = vcml::ENDIAN_LITTLE;
    else
        endian = vcml::ENDIAN_UNKNOWN;

    return is;
}

std::ostream& operator << (std::ostream& os, vcml::endianess& endian) {
    return os << vcml::endian_to_str(endian);
}
