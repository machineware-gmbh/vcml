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

#ifndef VCML_STRINGS_H
#define VCML_STRINGS_H

#include <stdarg.h>
#include <string.h>
#include <string>
#include <sstream>

#include "vcml/core/types.h"

namespace vcml {

using std::string;
VCML_TYPEINFO(string);

using std::stringstream;
using std::ostringstream;
using std::istringstream;

string mkstr(const char* format, ...) VCML_DECL_PRINTF(1, 2);
string vmkstr(const char* format, va_list args);
string trim(const string& s);
string to_lower(const string& s);
string to_upper(const string& s);
string escape(const string& s, const string& chars);
string unescape(const string& s);

vector<string> split(const string& str, const function<int(int)>& f = isspace);
vector<string> split(const string& str, char predicate);

template <typename V, typename T>
inline string join(const V& v, const T& separator) {
    if (v.empty())
        return "";

    ostringstream os;
    os << *v.begin();
    for (auto it = std::next(v.begin()); it != v.end(); it++)
        os << separator << *it;

    return os.str();
}

size_t replace(string& str, const string& s1, const string& s2);

template <typename T>
inline string to_string(const T& t) {
    ostringstream ss;
    ss << t;
    return ss.str();
}

template <>
inline string to_string<bool>(const bool& b) {
    return b ? "true" : "false";
}

template <>
inline string to_string<string>(const string& s) {
    return s;
}

inline string to_string(const char* s) {
    return s;
}

template <>
inline string to_string<u8>(const u8& v) {
    ostringstream ss;
    ss << (unsigned int)v;
    return ss.str();
}

template <typename T>
inline T from_string(const string& str) {
    if (str.empty())
        return T();

    istringstream ss;
    ss.str(str);
    ss.unsetf(std::ios::dec);
    ss.unsetf(std::ios::hex);
    ss.unsetf(std::ios::oct);

    T val = T();
    ss >> val;
    return val;
}

template <>
inline string from_string<string>(const string& s) {
    return s;
}

template <>
inline u8 from_string<u8>(const string& str) {
    if (str.empty())
        return 0;

    istringstream ss;
    ss.str(str);
    ss.unsetf(std::ios::dec);
    ss.unsetf(std::ios::hex);
    ss.unsetf(std::ios::oct);

    unsigned int val = 0;
    ss >> val;
    return (u8)val;
}

template <>
inline bool from_string<bool>(const string& str) {
    const string lower = to_lower(str);
    if (lower == "true")
        return true;
    if (lower == "false")
        return false;
    return from_string<unsigned int>(str) > 0;
}

inline char to_hex_ascii(unsigned int hex) {
    static const char hexchars[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    return hexchars[hex & 0xf];
}

inline unsigned int from_hex_ascii(char ch) {
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    return ch == '\0' ? 0 : -1;
}

inline bool contains(const string& s, const string& search) {
    return s.find(search) != std::string::npos;
}

inline bool starts_with(const string& s, const string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

inline bool ends_with(const string& s, const string& suffix) {
    if (s.size() < suffix.size())
        return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool is_number(const string& s) {
    if (s.empty())
        return false;
    if (starts_with(s, "0x"))
        return true;
    return std::all_of(s.begin(), s.end(), isdigit);
}

template <typename A, typename B>
inline string strcat(const A& a, const B& b) {
    return to_string(a) + to_string(b);
}

template <typename T, typename... ARGS>
inline string strcat(const T& arg, const ARGS&... args) {
    return to_string(arg) + strcat(args...);
}

} // namespace vcml

#endif
