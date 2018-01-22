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

#include "vcml/properties/property_provider_file.h"

namespace vcml {

    void property_provider_file::parse_file(const string& filename)
    {
        int lno = 0;
        string line, buffer = "";

        m_filename = filename;
        ifstream file(filename.c_str());

        while (std::getline(file, line)) {
            lno++;

            /* remove white spaces */
            line.erase(std::remove_if(line.begin(), line.end(), isspace),
                    line.end());
            if (line.empty())
                continue;

            /* continue on next line? */
            if (line[line.size() -1] == '\\') {
                buffer += line.substr(0, line.size() - 1);
                continue;
            }

            /* prepend buffer from previous lines */
            line = buffer + line;
            buffer = "";

            /* remove comments */
            size_t pos = line.find('#');
            if (pos != line.npos)
                line.erase(pos);
            if (line.empty())
                continue;

            /* read key=value part */
            size_t separator = line.find('=');
            if (separator == line.npos)
                log_warning("%s:%d missing '='", m_filename.c_str(), lno);

            string key = line.substr(0, separator);
            string val = line.substr(separator + 1);

            replace(val);
            add(key, val);
        }
    }

    void property_provider_file::replace(string& str) {
        std::map<string, string>::const_iterator it;
        for (it = m_replacements.begin(); it != m_replacements.end(); it++)
            vcml::replace(str, it->first, it->second);
    }

    property_provider_file::property_provider_file(const string& filename):
        property_provider(),
        m_filename(filename),
        m_replacements() {
        m_replacements["$dir"] = dirname(filename);
        parse_file(filename);
    }

    property_provider_file::~property_provider_file() {
        /* nothing to do */
    }

}
