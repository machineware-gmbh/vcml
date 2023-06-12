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

class command_base
{
private:
    string m_name;
    string m_desc;
    unsigned int m_argc;

public:
    const char* name() const { return m_name.c_str(); }
    const char* desc() const { return m_desc.c_str(); }
    unsigned int argc() const { return m_argc; }

    command_base(const string& name, unsigned int argc,
                 const string& desc = ""):
        m_name(name), m_desc(desc), m_argc(argc) {}

    virtual ~command_base() = default;

    virtual bool execute(const vector<string>& args, ostream& os) = 0;
};

template <class T>
class command : public command_base
{
private:
    T* m_host;
    bool (T::*m_func)(const vector<string>& args, ostream& os);

    // disabled
    command();
    command(const command<T>& other);

public:
    command(const string& name, unsigned int argc, const string& desc, T* host,
            bool (T::*func)(const vector<string>& args, ostream& os)):
        command_base(name, argc, desc), m_host(host), m_func(func) {}

    virtual ~command() = default;

    virtual bool execute(const vector<string>& args, ostream& os) {
        return (m_host->*m_func)(args, os);
    }
};

} // namespace vcml

#endif
