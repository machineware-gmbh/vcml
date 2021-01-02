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

#include <stdint.h>
#include <time.h>

#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <bitset>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

namespace vcml {

    typedef uint8_t  u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;

    typedef int8_t  i8;
    typedef int16_t i16;
    typedef int32_t i32;
    typedef int64_t i64;

    using ::clock_t;

    const clock_t  Hz = 1;
    const clock_t kHz = 1000 *  Hz;
    const clock_t MHz = 1000 * kHz;
    const clock_t GHz = 1000 * MHz;
    const clock_t THz = 1000 * GHz;

    using std::size_t;

    const size_t KiB = 1024;
    const size_t MiB = 1024 * KiB;
    const size_t GiB = 1024 * MiB;
    const size_t TiB = 1024 * GiB;

    template <typename T> struct typeinfo {
        static const char* name() { return "unknown"; }
    };

#define VCML_TYPEINFO(T)                         \
    template <> struct typeinfo<T> {             \
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
    using std::vector;
    using std::set;
    using std::unordered_set;
    using std::unordered_map;
    using std::bitset;

    template <typename V, typename T>
    inline void stl_remove_erase(V& v, const T& t) {
        v.erase(std::remove(v.begin(), v.end(), t), v.end());
    }

    template <typename V, typename T>
    inline bool stl_contains(const V& v, const T& t) {
        return std::find(v.begin(), v.end(), t) != v.end();
    }

    template <typename T1, typename T2>
    inline bool stl_contains(const std::map<T1,T2>& m, const T1& t) {
        return m.find(t) != m.end();
    }

    template <typename T1, typename T2>
    inline bool stl_contains(const std::unordered_map<T1,T2>& m, const T1& t) {
        return m.find(t) != m.end();
    }

    template <typename T>
    inline void stl_add_unique(vector<T>& v, const T& t) {
        if (!stl_contains(v, t))
            v.push_back(t);
    }

    using std::ostream;
    using std::fstream;
    using std::ifstream;
    using std::ofstream;

    using std::shared_ptr;
    using std::unique_ptr;

    using std::function;

    using std::atomic;

    using std::mutex;
    using std::lock_guard;
    using std::thread;

    inline bool is_set(int flags, int set) {
        return (flags & set) == set;
    }

    enum vcml_access {
        VCML_ACCESS_NONE       = 0x0,
        VCML_ACCESS_READ       = 0x1,
        VCML_ACCESS_WRITE      = 0x2,
        VCML_ACCESS_READ_WRITE = VCML_ACCESS_READ | VCML_ACCESS_WRITE
    };

    inline bool is_read_allowed(int a) {
        return is_set(a, VCML_ACCESS_READ);
    }

    inline bool is_write_allowed(int a) {
        return is_set(a, VCML_ACCESS_WRITE);
    }

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

std::istream& operator >> (std::istream& is, vcml::vcml_endian& endian);
std::ostream& operator << (std::ostream& os, vcml::vcml_endian& endian);

#endif
