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

#include "vcml/logging/logger.h"
#include "vcml/properties/broker_file.h"

namespace vcml {

void broker_file::parse_file(const string& filename) {
    size_t lno = 0;
    string line, buffer;
    ifstream file(filename.c_str());

    VCML_ERROR_ON(!file.good(), "cannot read '%s'", filename.c_str());

    while (std::getline(file, line)) {
        lno++;

        // remove white spaces
        line = trim(line);

        if (line.empty())
            continue;

        // continue on next line?
        if (line[line.size() - 1] == '\\') {
            buffer += line.substr(0, line.size() - 1);
            continue;
        }

        // prepend buffer from previous lines
        line   = strcat(buffer, line);
        buffer = "";

        // remove comments
        size_t pos = line.find('#');
        if (pos != line.npos)
            line.erase(pos);
        if (line.empty())
            continue;

        // check for include directive
        if (starts_with(line, "%include ")) {
            parse_file(trim(line.substr(9)));
            continue;
        }

        // check for loop directive
        if (starts_with(line, "for ")) {
            parse_loop(trim(line), filename, lno);
            continue;
        }

        // check for end-loop directive
        if (starts_with(line, "end")) {
            parse_done(trim(line), filename, lno);
            continue;
        }

        // read key=value part
        size_t separator = line.find('=');
        if (separator == line.npos)
            log_warn("%s:%zu missing '='", filename.c_str(), lno);

        string key = line.substr(0, separator);
        string val = line.substr(separator + 1);

        key = trim(key);
        val = trim(val);

        if (!key.empty())
            resolve(key, val, filename, lno);
    }

    for (auto loop : m_loops) {
        m_errors++;
        log_error("%s:%zu unmatched 'for'", loop.file.c_str(), loop.line);
    }
}

void broker_file::parse_loop(const string& expr, const string& file,
                             size_t line) {
    loopdesc loop;
    loop.file = file;
    loop.line = line;

    // parse 'for ITER : BOUNDS do'
    size_t offset = strlen("for ");
    size_t delim0 = expr.rfind(':');
    size_t delim1 = expr.rfind("do");
    string bounds = trim(expr.substr(delim0 + 1, delim1 - delim0 - 1));

    if (delim0 == expr.npos || delim1 == expr.npos || delim0 >= delim1) {
        m_errors++;
        log_error("%s:%zu error parsing loop", file.c_str(), line);
        return;
    }

    // parse ITER
    loop.iter = trim(expr.substr(offset, delim0 - offset - 1));

    // parse BOUNDS
    replace(bounds, file, line);

    size_t start = 0;
    size_t limit = from_string<size_t>(bounds);
    for (size_t i = start; i < limit; i++)
        loop.values.push_back(to_string(i));

    m_loops.push_front(loop);
}

void broker_file::parse_done(const string& expr, const string& file,
                             size_t line) {
    if (m_loops.empty()) {
        m_errors++;
        log_error("%s:%zu unmatched 'end'", file.c_str(), line);
        return;
    }

    m_loops.pop_front();
}

void broker_file::replace(string& str, const string& file, size_t line) {
    for (const auto& it : m_replacements)
        vcml::replace(str, it.first, it.second);

    size_t pos = 0;
    while ((pos = str.find("${", pos)) != str.npos) {
        size_t end = str.find('}', pos + 2);
        if (end == str.npos) {
            m_errors++;
            log_warn("%s:%zu missing '}'", file.c_str(), line);
            break;
        }

        string val = "";
        string key = str.substr(pos + 2, end - pos - 2);

        if (!lookup(key, val)) {
            m_errors++;
            log_warn("%s:%zu %s not defined", file.c_str(), line, key.c_str());
        }

        str = str.substr(0, pos) + val + str.substr(end + 1);
    }
}

void broker_file::resolve(const string& key, const string& val,
                          const string& file, size_t line) {
    if (m_loops.empty()) {
        string _key = key;
        string _val = val;
        replace(_key, file, line);
        replace(_val, file, line);
        define(_key, _val);
    } else {
        auto loop = m_loops.front();
        m_loops.pop_front();
        for (string iter : loop.values) {
            define(loop.iter, iter);
            resolve(key, val, file, line);
        }
        m_loops.push_front(loop);
    }
}

broker_file::broker_file(const string& file):
    broker(file, PRIO_CFGFILE),
    m_errors(0),
    m_filename(file),
    m_replacements() {
    m_replacements["$dir"] = m_replacements["${dir}"] = dirname(file);
    m_replacements["$cfg"] = m_replacements["${cfg}"] = filename_noext(file);
    m_replacements["$app"] = m_replacements["${app}"] = progname();
    m_replacements["$pwd"] = m_replacements["${pwd}"] = curr_dir();
    m_replacements["$tmp"] = m_replacements["${tmp}"] = temp_dir();
    m_replacements["$usr"] = m_replacements["${usr}"] = username();
    m_replacements["$pid"] = m_replacements["${pid}"] = to_string(getpid());
    parse_file(file);
    VCML_ERROR_ON(m_errors, "%zu errors parsing %s", m_errors, file.c_str());
}

broker_file::~broker_file() {
    // nothing to do
}

} // namespace vcml
