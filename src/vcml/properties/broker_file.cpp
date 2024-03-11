/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/logger.h"
#include "vcml/properties/broker_file.h"

namespace vcml {

void broker_file::parse_file(const string& file) {
    if (!mwr::file_exists(file)) {
        log_error("no such file: %s", file.c_str());
        m_errors++;
        return;
    }

    size_t lno = 0;
    string line, prev;
    ifstream stream(file.c_str());

    if (!stream.good()) {
        log_error("cannot read '%s'", file.c_str());
        m_errors++;
        return;
    }

    try {
        while (std::getline(stream, line)) {
            lno++;

            // remove comments
            size_t pos = line.find('#');
            if (pos != line.npos)
                line.erase(pos);

            // remove white spaces
            line = rtrim(line);

            // prepend buffer from previous lines
            line = strcat(prev, line);
            prev = "";

            if (line.empty())
                continue;

            // continue on next line?
            if (line[line.size() - 1] == '\\') {
                prev += line.substr(0, line.size() - 1);
                continue;
            }

            // trim again and parse
            parse_expr(ltrim(line), file, lno);
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

void broker_file::parse_expr(const string& expr, const string& file,
                             size_t line) {
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
    define("dir", mwr::escape(mwr::dirname(file), ""), 1);
    define("cfg", mwr::escape(mwr::filename_noext(file), ""), 1);

    parse_file(file);

    if (m_errors > 1)
        VCML_REPORT("%zu errors parsing %s", m_errors, file.c_str());
    else if (m_errors)
        VCML_REPORT("error parsing %s", file.c_str());
}

broker_file::~broker_file() {
    // nothing to do
}

} // namespace vcml
