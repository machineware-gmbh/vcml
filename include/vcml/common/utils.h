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

#ifndef VCML_UTILS_H
#define VCML_UTILS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/thctl.h"

namespace vcml {

    const size_t KiB = 1024;
    const size_t MiB = 1024 * KiB;
    const size_t GiB = 1024 * MiB;
    const size_t TiB = 1024 * GiB;

#define VCML_KIND(name)                \
    virtual const char* kind() const { \
        return "vcml::" #name;         \
    }

    string mkstr(const char* format, ...);
    string vmkstr(const char* format, va_list args);
    string concat(const string& a, const string& b);
    string dirname(const string& filename);
    string tempdir();

    string progname();
    string username();

    double realtime();

    bool file_exists(const string& filename);

    void trim(string& s);

    string tlm_response_to_str(tlm_response_status status);
    string tlm_transaction_to_str(const tlm_generic_payload& tx);

    sc_object*    find_object(const string& name);
    sc_attr_base* find_attribute(const string& name);

    int replace(string& str, const string& s1, const string& s2);

    string call_origin();

    vector<string> backtrace(unsigned int frames = 63, unsigned int skip = 1);

    bool is_debug_build();

    inline string escape(const string& s, const string& chars) {
        stringstream ss;
        for (auto c : s) {
            for (auto esc : chars + "\\") {
                if (c == esc)
                    ss << '\\';
            }
            ss << c;
        }
        return ss.str();
    }

    inline string unescape(const string& s) {
        stringstream ss;
        for (auto c : s) {
            if (c != '\\')
                ss << c;
        }
        return ss.str();
    }

    inline vector<string> split(const string& str, std::function<int(int)> f) {
        vector<string> vec;
        string buf = "";
        for (auto ch : str) {
            if (f(ch)) {
                if (!buf.empty()) {
                    vec.push_back(buf);
                    buf = "";
                }
            } else {
                buf += ch;
            }
        }
        if (!buf.empty())
            vec.push_back(buf);
        return vec;
    }

    inline vector<string> split(const string& str, char predicate) {
        vector<string> vec;
        string buf = "";
        for (unsigned int i = 0; i < str.length(); i++) {
            char ch = str[i];
            if (ch == '\\')
                buf += str[++i];
            else if (ch == predicate) {
                if (!buf.empty()) {
                    vec.push_back(buf);
                    buf = "";
                }
            } else {
                buf += ch;
            }
        }
        if (!buf.empty())
            vec.push_back(buf);
        return vec;
    }

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

    template <typename T>
    inline void stl_add_unique(vector<T>& v, const T& t) {
        if (!stl_contains(v, t))
            v.push_back(t);
    }

    template <typename T>
    inline string to_string(const T& t) {
        stringstream ss;
        ss << t;
        return ss.str();
    }

    template <>
    inline string to_string<string>(const string& s) {
        return s;
    }

    template <>
    inline string to_string<u8>(const u8& v) {
        stringstream ss;
        ss << (unsigned int)v;
        return ss.str();
    }

    template <typename T>
    inline T from_string(const string& str) {
        stringstream ss; ss.str(str);
        ss.unsetf(std::ios::dec);
        ss.unsetf(std::ios::hex);
        ss.unsetf(std::ios::oct);

        T val;
        ss >> val;
        return val;
    }

    template <>
    inline string from_string<string>(const string& s) {
        return s;
    }

    template <>
    inline u8 from_string<u8>(const string& str) {
        stringstream ss; ss.str(str);
        ss.unsetf(std::ios::dec);
        ss.unsetf(std::ios::hex);
        ss.unsetf(std::ios::oct);

        unsigned int val;
        ss >> val;
        return (u8)val;
    }

    template <typename DATA>
    DATA bswap(DATA val);

    template <>
    inline u8 bswap<u8>(u8 val) {
        return val;
    }

    template <>
    inline u16 bswap<u16>(u16 val) {
        return ((val & 0xff00) >> 8) |
               ((val & 0x00ff) << 8);
    }

    template <>
    inline u32 bswap<u32>(u32 val) {
        return ((val & 0xff000000) >> 24) |
               ((val & 0x00ff0000) >>  8) |
               ((val & 0x0000ff00) <<  8) |
               ((val & 0x000000ff) << 24);
    }

    template <>
    inline u64 bswap<u64>(u64 val) {
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

#if (SYSTEMC_VERSION < 20140417)
inline sc_core::sc_time operator % (const sc_core::sc_time& t1,
                                    const sc_core::sc_time& t2 ) {
    sc_core::sc_time tmp(t1.value() % t2.value(), false);
    return tmp;
}
#endif

std::istream& operator >> (std::istream& is, vcml::vcml_endian& endian);
std::istream& operator >> (std::istream& is, sc_core::sc_time& t);

#endif
