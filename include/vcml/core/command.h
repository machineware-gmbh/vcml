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

#ifndef VCML_COMMAND_H
#define VCML_COMMAND_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"

#define VCML_NO_ARGS (::std::vector<::std::string>())

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
