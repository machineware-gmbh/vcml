/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_SERIAL_BACKEND_H
#define VCML_SERIAL_BACKEND_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"

namespace vcml { namespace serial {

    class backend
    {
    private:
        string m_port;

    protected:
        string m_type;

    public:
        backend(const string& port);
        virtual ~backend();

        backend() = delete;
        backend(const backend&) = delete;
        backend(backend&&) = default;

        const char* port() const { return m_port.c_str(); }
        const char* type() const { return m_type.c_str(); }

        virtual bool read(u8& val) = 0;
        virtual void write(u8 val) = 0;

        static backend* create(const string& port, const string& type);
    };

}}

#endif
