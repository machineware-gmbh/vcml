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

#ifndef VCML_BLOCK_BACKEND_RAM_H
#define VCML_BLOCK_BACKEND_RAM_H

#include "vcml/core/types.h"

#include "vcml/models/block/backend.h"

namespace vcml {
namespace block {

class backend_ram : public backend
{
protected:
    size_t m_capacity;
    u8* m_buf;
    u8* m_pos;
    u8* m_end;

public:
    backend_ram(size_t cap, bool readonly);
    virtual ~backend_ram();

    virtual size_t capacity();
    virtual size_t pos();

    virtual void seek(size_t pos);
    virtual void read(vector<u8>& buffer);
    virtual void write(const vector<u8>& buffer);
    virtual void write(u8 data, size_t count);
    virtual void save(ostream& os);
};

} // namespace block
} // namespace vcml

#endif
