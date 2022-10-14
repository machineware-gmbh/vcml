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

void broker_file::parse_file(const string& file) {
    size_t lno = 0;
    string line, prev;
    ifstream stream(file.c_str());

    VCML_ERROR_ON(!stream.good(), "cannot read '%s'", file.c_str());

    try {
        while (std::getline(stream, line)) {
            lno++;

            // remove white spaces
            line = trim(line);

            if (line.empty())
                continue;

            // continue on next line?
            if (line[line.size() - 1] == '\\') {
                prev += line.substr(0, line.size() - 1);
                continue;
            }

            // prepend buffer from previous lines
            line = strcat(prev, line);
            prev = "";

            parse_expr(line, file, lno);
        }
    } catch (std::exception& ex) {
        m_errors++;
        log_error("%s:%zu %s", file.c_str(), lno, ex.what());
    }

    for (const auto& loop : m_loops) {
        m_errors++;
        log_error("%s:%zu unmatched 'for'", loop.file.c_str(), loop.line);
    }
}

void broker_file::parse_expr(string expr, const string& file, size_t line) {
    // remove comments
    size_t pos = expr.find('#');
    if (pos != expr.npos)
        expr.erase(pos);
    if (expr.empty())
        return;

    // check for include directive
    if (starts_with(expr, "%include ")) {
        string incl = expand(trim(expr.substr(9)));
        define("dir", mwr::dirname(incl), 1);
        parse_file(incl);
        define("dir", mwr::dirname(file), 1);
        return;
    }

    // check for loop directive
    if (starts_with(expr, "for ")) {
        parse_loop(trim(expr), file, line);
        return;
    }

    // check for end-loop directive
    if (starts_with(expr, "done")) {
        parse_done(trim(expr), file, line);
        return;
    }

    // read key=value part
    size_t separator = expr.find('=');
    if (separator == expr.npos)
        VCML_REPORT("%s:%zu missing '='", file.c_str(), line);

    string key = expr.substr(0, separator);
    string val = expr.substr(separator + 1);

    key = trim(key);
    val = trim(val);

    if (!key.empty())
        resolve(key, val, file, line);
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

    if (delim0 == expr.npos || delim1 == expr.npos || delim0 >= delim1) {
        m_errors++;
        log_error("%s:%zu error parsing loop", file.c_str(), line);
        return;
    }

    // parse ITER
    loop.iter = trim(expr.substr(offset, delim0 - offset - 1));

    // parse BOUNDS
    string bounds = expand(trim(expr.substr(delim0 + 1, delim1 - delim0 - 1)));
    size_t start = 0, limit = from_string<size_t>(bounds);

    for (size_t i = start; i < limit; i++)
        loop.values.push_back(to_string(i));

    m_loops.push_front(loop);
}

void broker_file::parse_done(const string& expr, const string& file,
                             size_t line) {
    if (m_loops.empty()) {
        m_errors++;
        log_error("%s:%zu unmatched '%s'", file.c_str(), line, expr.c_str());
        return;
    }

    m_loops.pop_front();
}

void broker_file::resolve(const string& key, const string& val,
                          const string& file, size_t line) {
    if (m_loops.empty()) {
        define(key, val);
        return;
    }

    auto loop = m_loops.front();
    m_loops.pop_front();

    for (const string& iter : loop.values) {
        define(loop.iter, iter);
        resolve(key, val, file, line);
    }

    m_loops.push_front(loop);
}

broker_file::broker_file(const string& file):
    broker(file), m_errors(0), m_filename(file) {
    define("dir", mwr::dirname(file), 1);
    define("cfg", mwr::filename_noext(file), 1);

    parse_file(file);

    VCML_ERROR_ON(m_errors, "%zu errors parsing %s", m_errors, file.c_str());
}

broker_file::~broker_file() {
    // nothing to do
}

} // namespace vcml
