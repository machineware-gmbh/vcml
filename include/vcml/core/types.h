/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TYPES_H
#define VCML_TYPES_H

#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <bitset>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <variant>
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <initializer_list>
#include <typeindex>

#include <stdint.h>
#include <limits.h>
#include <time.h>

#include <mwr.h>

#if defined(MWR_MSVC)
#pragma warning(disable : 4200) // nonstandard extension used
#pragma warning(disable : 4244) // implicit cast to smaller type
#pragma warning(disable : 4267) // return cast to smaller type
#pragma warning(disable : 4996) // deprecated stdlib warnings
#endif

#define VCML_ERROR     MWR_ERROR
#define VCML_ERROR_ON  MWR_ERROR_ON
#define VCML_REPORT    MWR_REPORT
#define VCML_REPORT_ON MWR_REPORT_ON

namespace vcml {

using mwr::u8;
using mwr::u16;
using mwr::u32;
using mwr::u64;

using mwr::i8;
using mwr::i16;
using mwr::i32;
using mwr::i64;

using mwr::f32;
using mwr::f64;

using mwr::string;
using mwr::size_t;
using mwr::hz_t;

using mwr::Hz;
using mwr::kHz;
using mwr::MHz;
using mwr::GHz;
using mwr::THz;

using mwr::KiB;
using mwr::MiB;
using mwr::GiB;
using mwr::TiB;

template <typename T>
struct typeinfo {
    static const char* name() { return "unknown"; }
};

template <typename T>
inline const char* type_name() {
    return typeinfo<T>::name();
}

#define VCML_TYPEINFO(T)            \
    template <>                     \
    struct typeinfo<T> {            \
        static const char* name() { \
            return #T;              \
        }                           \
    }

VCML_TYPEINFO(u8);
VCML_TYPEINFO(u16);
VCML_TYPEINFO(u32);
VCML_TYPEINFO(u64);

VCML_TYPEINFO(i8);
VCML_TYPEINFO(i16);
VCML_TYPEINFO(i32);
VCML_TYPEINFO(i64);

VCML_TYPEINFO(bool);
VCML_TYPEINFO(float);
VCML_TYPEINFO(double);
VCML_TYPEINFO(string);

#ifndef MWR_MSVC
VCML_TYPEINFO(size_t);
#endif

using std::min;
using std::max;

using std::list;
using std::queue;
using std::deque;
using std::priority_queue;
using std::array;
using std::vector;
using std::set;
using std::unordered_set;
using std::unordered_map;
using std::pair;
using std::bitset;
using std::initializer_list;

using std::istream;
using std::ostream;
using std::fstream;
using std::ifstream;
using std::ofstream;

using std::shared_ptr;
using std::unique_ptr;

using std::variant;
using std::optional;

using std::function;

using std::atomic;

using std::mutex;
using std::lock_guard;
using std::condition_variable;
using std::condition_variable_any;
using std::thread;

using std::stringstream;
using std::ostringstream;
using std::istringstream;

using mwr::report;

using mwr::mkstr;
using mwr::vmkstr;
using mwr::ltrim;
using mwr::rtrim;
using mwr::trim;
using mwr::to_lower;
using mwr::to_upper;
using mwr::escape;
using mwr::unescape;
using mwr::split;
using mwr::join;
using mwr::replace;
using mwr::to_string;
using mwr::from_string;
using mwr::to_hex_ascii;
using mwr::from_hex_ascii;
using mwr::contains;
using mwr::starts_with;
using mwr::ends_with;
using mwr::is_number;
using mwr::strcat;

using mwr::stl_remove;
using mwr::stl_remove_if;
using mwr::stl_remove_if;
using mwr::stl_remove_if;
using mwr::stl_contains;
using mwr::stl_contains;
using mwr::stl_contains;
using mwr::stl_contains_if;
using mwr::stl_add_unique;

using mwr::stream_guard;

using mwr::width_of;
using mwr::popcnt;
using mwr::parity;
using mwr::parity_odd;
using mwr::parity_even;
using mwr::is_pow2;
using mwr::clz;
using mwr::ctz;
using mwr::ffs;
using mwr::fls;
using mwr::bit;
using mwr::bitmask;
using mwr::fourcc;
using mwr::bitrev;
using mwr::bswap;
using mwr::memswap;
using mwr::crc7;
using mwr::crc16;
using mwr::crc32;

using mwr::extract;
using mwr::sextract;
using mwr::signext;
using mwr::deposit;
using mwr::field;
using mwr::get_field;
using mwr::set_field;
using mwr::set_bit;

using mwr::log_level;

VCML_TYPEINFO(log_level);

using mwr::LOG_DEBUG;
using mwr::LOG_INFO;
using mwr::LOG_WARN;
using mwr::LOG_ERROR;

using mwr::STDIN_FDNO;
using mwr::STDOUT_FDNO;
using mwr::STDERR_FDNO;

bool is_debug_build();

inline bool is_set(int flags, int set) {
    return (flags & set) == set;
}

enum vcml_access {
    VCML_ACCESS_NONE = 0x0,
    VCML_ACCESS_READ = 0x1,
    VCML_ACCESS_WRITE = 0x2,
    VCML_ACCESS_READ_WRITE = VCML_ACCESS_READ | VCML_ACCESS_WRITE
};

inline bool is_read_allowed(int a) {
    return is_set(a, VCML_ACCESS_READ);
}

inline bool is_write_allowed(int a) {
    return is_set(a, VCML_ACCESS_WRITE);
}

typedef enum vcml_endianess {
    ENDIAN_UNKNOWN = 0,
    ENDIAN_LITTLE = 1,
    ENDIAN_BIG = 2,
} endianess;

VCML_TYPEINFO(endianess);

istream& operator>>(istream& is, endianess& e);
ostream& operator<<(ostream& os, endianess e);

inline endianess host_endian() {
    u32 test = 1;
    u8* p = reinterpret_cast<u8*>(&test);
    if (p[0] == 1)
        return ENDIAN_LITTLE;
    if (p[3] == 1)
        return ENDIAN_BIG;
    return ENDIAN_UNKNOWN;
}

typedef unsigned int address_space;

enum vcml_address_space : address_space {
    VCML_AS_DEFAULT = 0,
};

typedef enum vcml_alignment {
    VCML_ALIGN_NONE = 0,
    VCML_ALIGN_1K = 10,
    VCML_ALIGN_2K = 11,
    VCML_ALIGN_4K = 12,
    VCML_ALIGN_8K = 13,
    VCML_ALIGN_16K = 14,
    VCML_ALIGN_32K = 15,
    VCML_ALIGN_64K = 16,
    VCML_ALIGN_128K = 17,
    VCML_ALIGN_256K = 18,
    VCML_ALIGN_512K = 19,
    VCML_ALIGN_1M = 20,
    VCML_ALIGN_2M = 21,
    VCML_ALIGN_4M = 22,
    VCML_ALIGN_8M = 23,
    VCML_ALIGN_16M = 24,
    VCML_ALIGN_32M = 25,
    VCML_ALIGN_64M = 26,
    VCML_ALIGN_128M = 27,
    VCML_ALIGN_256M = 28,
    VCML_ALIGN_512M = 29,
    VCML_ALIGN_1G = 30,
} alignment;

VCML_TYPEINFO(alignment);

inline vcml_alignment host_page_alignment() {
    static const size_t pgsz = mwr::get_page_size();
    return pgsz ? (vcml_alignment)ctz(pgsz) : VCML_ALIGN_NONE;
}

istream& operator>>(istream& is, alignment& a);
ostream& operator<<(ostream& os, alignment a);

template <typename T>
constexpr bool is_aligned(T addr, alignment a) {
    return ((u64)addr & ((1ull << a) - 1)) == 0;
}

} // namespace vcml

#endif
