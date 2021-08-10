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
        int lno = 0;
        string line, buffer = "";
        ifstream file(filename.c_str());

        VCML_ERROR_ON(!file.good(), "cannot read '%s'", filename.c_str());

        while (std::getline(file, line)) {
            lno++;

            // remove white spaces
            line = trim(line);

            if (line.empty())
                continue;

            // continue on next line?
            if (line[line.size() -1] == '\\') {
                buffer += line.substr(0, line.size() - 1);
                continue;
            }

            // prepend buffer from previous lines
            line = buffer + line;
            buffer = "";

            // remove comments
            size_t pos = line.find('#');
            if (pos != line.npos)
                line.erase(pos);
            if (line.empty())
                continue;

            // read key=value part
            size_t separator = line.find('=');
            if (separator == line.npos)
                log_warn("%s:%d missing '='", m_filename.c_str(), lno);

            string key = line.substr(0, separator);
            string val = line.substr(separator + 1);

            key = trim(key);
            val = trim(val);

            replace(val);
            insert(key, val);
        }
    }

    void broker_file::replace(string& str) {
        for (auto it : m_replacements)
            vcml::replace(str, it.first, it.second);
    }

    broker_file::broker_file(const string& filepath):
        broker(filepath, PRIO_CFGFILE),
        m_filename(filepath),
        m_replacements() {
        m_replacements["$dir"] = dirname(filepath);
        m_replacements["$cfg"] = filename_noext(filepath);
        m_replacements["$app"] = progname();
        m_replacements["$pwd"] = curr_dir();
        m_replacements["$tmp"] = temp_dir();
        m_replacements["$usr"] = username();
        m_replacements["$pid"] = to_string(getpid());
        parse_file(filepath);
    }

    broker_file::~broker_file() {
        // nothing to do
    }

}
