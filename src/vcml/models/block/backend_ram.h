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
    enum : size_t {
        SECTOR_SIZE = 512,
    };

    size_t m_pos;
    size_t m_cap;
    std::map<u64, u8*> m_sectors;

    u8* get_page(u64 addr);

public:
    backend_ram(size_t cap, bool readonly);
    virtual ~backend_ram();

    virtual size_t capacity() override;
    virtual size_t pos() override;

    virtual void seek(size_t pos) override;
    virtual void read(u8* buffer, size_t size) override;
    virtual void write(const u8* buffer, size_t size) override;
    virtual void write(u8 data, size_t count) override;
    virtual void save(ostream& os) override;
};

} // namespace block
} // namespace vcml

#endif
