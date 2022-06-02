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

#include <stdio.h> // for vsnprintf
#include <ctype.h> // for isspace, tolower, toupper

#include "vcml/common/strings.h"

namespace vcml {

string mkstr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    string str = vmkstr(format, args);
    va_end(args);
    return str;
}

string vmkstr(const char* format, va_list args) {
    va_list args2;
    va_copy(args2, args);

    int size = vsnprintf(NULL, 0, format, args) + 1;
    if (size <= 0) {
        va_end(args2);
        return "";
    }

    char* buffer = new char[size];
    vsnprintf(buffer, size, format, args2);
    va_end(args2);

    string s(buffer);
    delete[] buffer;
    return s;
}

string trim(const string& str) {
    static const auto nospace = [](int ch) { return !std::isspace(ch); };

    string copy(str);

    auto front = std::find_if(copy.begin(), copy.end(), nospace);
    copy.erase(copy.begin(), front);

    auto back = std::find_if(copy.rbegin(), copy.rend(), nospace);
    copy.erase(back.base(), copy.end());

    return copy;
}

string to_lower(const string& s) {
    string result;
    for (auto ch : s)
        result += tolower(ch);
    return result;
}

string to_upper(const string& s) {
    string result;
    for (auto ch : s)
        result += toupper(ch);
    return result;
}

string escape(const string& s, const string& chars) {
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

string unescape(const string& s) {
    stringstream ss;
    for (auto c : s) {
        if (c != '\\')
            ss << c;
    }

    return ss.str();
}

vector<string> split(const string& str, const function<int(int)>& f) {
    vector<string> vec;
    string buf;

    for (unsigned int i = 0; i < str.length(); i++) {
        char ch = str[i];
        if (ch == '\\' && i < str.length() - 1) {
            buf += str[++i];
        } else if (f(ch)) {
            if (!buf.empty())
                vec.push_back(buf);
            buf = "";
        } else {
            buf += ch;
        }
    }

    if (!buf.empty())
        vec.push_back(buf);

    return vec;
}

vector<string> split(const string& str, char predicate) {
    vector<string> vec;
    string buf;

    for (unsigned int i = 0; i < str.length(); i++) {
        char ch = str[i];
        if (ch == '\\' && i < str.length() - 1)
            buf += str[++i];
        else if (ch == predicate) {
            if (!buf.empty())
                vec.push_back(buf);
            buf = "";
        } else {
            buf += ch;
        }
    }

    if (!buf.empty())
        vec.push_back(buf);

    return vec;
}

size_t replace(string& str, const string& search, const string& repl) {
    size_t count = 0;
    size_t index = 0;

    while (true) {
        index = str.find(search, index);
        if (index == std::string::npos)
            break;

        str.replace(index, search.length(), repl);
        index += repl.length();
        count++;
    }

    return count;
}

} // namespace vcml
