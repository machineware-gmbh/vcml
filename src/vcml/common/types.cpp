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
        case VCML_ENDIAN_LITTLE: return "little";
        case VCML_ENDIAN_BIG: return "big";
        default:
            return "unknown";
        }
    }

}

std::istream& operator >> (std::istream& is, sc_core::sc_time& t) {
    std::string str;
    is >> str;

    char* endptr = NULL;
    sc_dt::uint64 value = strtoul(str.c_str(), &endptr, 0);
    double fval = value;

    if (strcmp(endptr, "ps") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_PS);
    else if (strcmp(endptr, "ns") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_NS);
    else if (strcmp(endptr, "us") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_US);
    else if (strcmp(endptr, "ms") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_MS);
    else if (strcmp(endptr, "s") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_SEC);
    else // raw value, not part of ieee1666!
        t = sc_core::sc_time(value, true);

    return is;
}

std::istream& operator >> (std::istream& is, vcml::vcml_endian& endian) {
    std::string str;
    is >> str;
    str = vcml::to_lower(str);

    if (str == "big")
        endian = vcml::VCML_ENDIAN_BIG;
    else if (str == "little")
        endian = vcml::VCML_ENDIAN_LITTLE;
    else
        endian = vcml::VCML_ENDIAN_UNKNOWN;

    return is;
}

std::ostream& operator << (std::ostream& os, vcml::vcml_endian& endian) {
    return os << vcml::endian_to_str(endian);
}
