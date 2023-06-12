/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    void parse_expr(const string& expr, const string& file, size_t line);
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
