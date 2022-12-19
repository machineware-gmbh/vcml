/******************************************************************************
 *                                                                            *
 * Copyright 2022 MachineWare GmbH                                            *
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

#ifndef VCML_BLOCK_BACKEND_H
#define VCML_BLOCK_BACKEND_H

#include "vcml/core/types.h"

namespace vcml {
namespace block {

class backend
{
protected:
    string m_type;
    bool m_readonly;

public:
    const char* type() const { return m_type.c_str(); }
    bool readonly() const { return m_readonly; }

    backend(const string& type, bool readonly);
    virtual ~backend() = default;

    backend() = delete;
    backend(const backend&) = delete;
    backend(backend&&) = default;

    virtual size_t capacity() = 0;
    virtual size_t pos() = 0;
    virtual size_t remaining();

    virtual void seek(size_t pos) = 0;
    virtual void read(u8* buffer, size_t size) = 0;
    virtual void write(const u8* buffer, size_t size) = 0;
    virtual void write(u8 data, size_t count) = 0;
    virtual void save(ostream& os) = 0;

    static backend* create(const string& image, bool readonly);
};

} // namespace block
} // namespace vcml

#endif
