/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/types.h"

namespace vcml {

istream& operator>>(istream& is, endianess& endian) {
    std::string str;
    is >> str;
    str = vcml::to_lower(str);

    if (str == "big")
        endian = ENDIAN_BIG;
    else if (str == "little")
        endian = ENDIAN_LITTLE;
    else
        endian = ENDIAN_UNKNOWN;

    return is;
}

ostream& operator<<(ostream& os, endianess e) {
    switch (e) {
    case ENDIAN_LITTLE:
        return os << "little";
    case ENDIAN_BIG:
        return os << "big";
    default:
        return os << "unknown";
    }
}

istream& operator>>(istream& is, alignment& a) {
    string s;
    is >> s;
    if (s.empty()) {
        a = VCML_ALIGN_NONE;
        return is;
    }

    char* endp;
    u64 val = strtoull(s.c_str(), &endp, 0);
    if (val == 0) {
        a = VCML_ALIGN_NONE;
        return is;
    }

    a = (alignment)ctz(val);

    switch (*endp) {
    case 'k':
    case 'K':
        a = (alignment)(a + 10);
        break;
    case 'm':
    case 'M':
        a = (alignment)(a + 20);
        break;
    case 'g':
    case 'G':
        a = (alignment)(a + 30);
        break;
    default:
        a = VCML_ALIGN_NONE;
        break;
    }

    return is;
}

ostream& operator<<(ostream& os, alignment a) {
    if (a == VCML_ALIGN_NONE)
        return os << "unaligned" << std::endl;
    if (a >= 30)
        return os << (1ull << (a - 30)) << "G";
    if (a >= 20)
        return os << (1ull << (a - 20)) << "M";
    if (a >= 10)
        return os << (1ull << (a - 10)) << "k";
    return os << (1ull << a);
}

} // namespace vcml
