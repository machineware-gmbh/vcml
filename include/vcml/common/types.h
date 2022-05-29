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

#ifndef VCML_TYPES_H
#define VCML_TYPES_H

#if __cplusplus < 201103L
#error Please compile with c++11
#endif

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
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#define VCML_DECL_PRINTF(strpos, argpos) \
    __attribute__((format(printf, (strpos), (argpos))))

#define VCML_DECL_PACKED __attribute__((packed))

namespace vcml {

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

using ::clock_t;

const clock_t Hz = 1;           // NOLINT(readability-identifier-naming)
const clock_t kHz = 1000 * Hz;  // NOLINT(readability-identifier-naming)
const clock_t MHz = 1000 * kHz; // NOLINT(readability-identifier-naming)
const clock_t GHz = 1000 * MHz; // NOLINT(readability-identifier-naming)
const clock_t THz = 1000 * GHz; // NOLINT(readability-identifier-naming)

using std::size_t;

const size_t KiB = 1024;       // NOLINT(readability-identifier-naming)
const size_t MiB = 1024 * KiB; // NOLINT(readability-identifier-naming)
const size_t GiB = 1024 * MiB; // NOLINT(readability-identifier-naming)
const size_t TiB = 1024 * GiB; // NOLINT(readability-identifier-naming)

typedef std::size_t id_t;

template <typename T>
struct typeinfo {
    static const char* name() { return "unknown"; }
};

#define VCML_TYPEINFO(T)                         \
    template <>                                  \
    struct typeinfo<T> {                         \
        static const char* name() { return #T; } \
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

template <typename T>
inline const char* type_name() {
    return typeinfo<T>::name();
}

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

template <typename V, typename T>
inline void stl_remove_erase(V& v, const T& t) {
    v.erase(std::remove(v.begin(), v.end(), t), v.end());
}

template <typename V, class PRED>
inline void stl_remove_erase_if(V& v, PRED p) {
    v.erase(std::remove_if(v.begin(), v.end(), p), v.end());
}

template <typename V, typename T>
inline bool stl_contains(const V& v, const T& t) {
    return std::find(v.begin(), v.end(), t) != v.end();
}

template <typename T1, typename T2>
inline bool stl_contains(const std::map<T1, T2>& m, const T1& t) {
    return m.find(t) != m.end();
}

template <typename T1, typename T2>
inline bool stl_contains(const std::unordered_map<T1, T2>& m, const T1& t) {
    return m.find(t) != m.end();
}

template <typename V, class PRED>
inline bool stl_contains_if(const V& v, PRED p) {
    return std::find_if(v.begin(), v.end(), p) != v.end();
}

template <typename T>
inline void stl_add_unique(vector<T>& v, const T& t) {
    if (!stl_contains(v, t))
        v.push_back(t);
}

using std::istream;
using std::ostream;
using std::fstream;
using std::ifstream;
using std::ofstream;

using std::shared_ptr;
using std::unique_ptr;

using std::optional;

using std::function;

using std::atomic;

using std::mutex;
using std::lock_guard;
using std::condition_variable;
using std::condition_variable_any;
using std::thread;

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

istream& operator>>(istream& is, alignment& a);
ostream& operator<<(ostream& os, alignment a);

template <typename T>
constexpr bool is_aligned(T addr, alignment a) {
    return ((u64)addr & ((1ull << a) - 1)) == 0;
}

} // namespace vcml

#endif
