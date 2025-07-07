/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_COMMAND_H
#define VCML_COMMAND_H

#include "vcml/core/types.h"

namespace vcml {

using command_func = std::function<bool(const vector<string>&, ostream& os)>;

class command
{
private:
    string m_name;
    string m_desc;
    unsigned int m_argc;
    command_func m_func;

public:
    const char* name() const { return m_name.c_str(); }
    const char* desc() const { return m_desc.c_str(); }
    unsigned int argc() const { return m_argc; }

    command(const string& name, unsigned int argc, command_func func,
            const string& desc = ""):
        m_name(name), m_desc(desc), m_argc(argc), m_func(std::move(func)) {}

    virtual ~command() = default;

    virtual bool execute(const vector<string>& args, ostream& os) {
        return m_func(args, os);
    }
};

} // namespace vcml

#endif
