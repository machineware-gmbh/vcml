/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    virtual void wzero(size_t size, bool may_unmap) override;
    virtual void discard(size_t size) override;
    virtual void save(ostream& os) override;
    virtual void flush() override;
};

} // namespace block
} // namespace vcml

#endif
