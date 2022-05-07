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

#ifndef VCML_BROKER_FILE_H
#define VCML_BROKER_FILE_H

#include "vcml/properties/broker.h"

namespace vcml {

class broker_file : public broker
{
private:
    size_t m_errors;
    string m_filename;

    struct loopdesc {
        string iter;
        vector<string> values;
        string file;
        size_t line;
    };

    std::deque<loopdesc> m_loops;

    void parse_file(const string& filename);
    void parse_expr(string expr, const string& file, size_t line);
    void parse_loop(const string& expr, const string& file, size_t line);
    void parse_done(const string& expr, const string& file, size_t line);

    void resolve(const string&, const string&, const string&, size_t);

public:
    broker_file() = delete;
    broker_file(const string& filename);
    virtual ~broker_file();
    VCML_KIND(broker_file);
};

} // namespace vcml

#endif
