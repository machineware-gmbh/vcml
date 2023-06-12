/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/block/backend_ram.h"

namespace vcml {
namespace block {

backend_ram::backend_ram(size_t cap, bool readonly):
    backend("ramdisk", readonly), m_pos(), m_cap(cap), m_sectors() {
}

backend_ram::~backend_ram() {
    for (const auto& sector : m_sectors)
        delete[] sector.second;
}

size_t backend_ram::capacity() {
    return m_cap;
}

size_t backend_ram::pos() {
    return m_pos;
}

void backend_ram::seek(size_t pos) {
    VCML_REPORT_ON(pos > capacity(), "attempt to seek beyond end of buffer");
    m_pos = pos;
}

void backend_ram::read(u8* buffer, size_t size) {
    if (m_pos + size > m_cap)
        VCML_REPORT("attempt to read beyond end of buffer");

    size_t done = 0;
    while (done < size) {
        size_t off = m_pos % SECTOR_SIZE;
        size_t num = min(SECTOR_SIZE - off, size - done);
        auto it = m_sectors.find(m_pos / SECTOR_SIZE);
        if (it == m_sectors.end())
            memset(buffer + done, 0, num);
        else
            memcpy(buffer + done, it->second + off, num);

        done += num;
        m_pos += num;
    }
}

void backend_ram::write(const u8* buffer, size_t size) {
    if (m_pos + size > m_cap)
        VCML_REPORT("attempt to write beyond end of buffer");

    size_t done = 0;
    while (done < size) {
        size_t off = m_pos % SECTOR_SIZE;
        size_t num = min(SECTOR_SIZE - off, size - done);
        u8*& sector = m_sectors[m_pos / SECTOR_SIZE];
        if (!sector)
            sector = new u8[SECTOR_SIZE]();

        memcpy(sector + off, buffer + done, num);
        m_pos += num;
        done += num;
    }
}

void backend_ram::wzero(size_t size, bool may_unmap) {
    if (m_pos + size > m_cap)
        VCML_REPORT("attempt to write beyond end of buffer");

    size_t done = 0;
    while (done < size) {
        size_t off = m_pos % SECTOR_SIZE;
        size_t num = min(SECTOR_SIZE - off, size - done);

        if (num == SECTOR_SIZE && may_unmap) {
            discard(SECTOR_SIZE);
            done += SECTOR_SIZE;
            continue;
        }

        u8*& sector = m_sectors[m_pos / SECTOR_SIZE];
        if (!sector)
            sector = new u8[SECTOR_SIZE]();
        else
            memset(sector + off, 0, num);

        m_pos += size;
        done += size;
    }
}

void backend_ram::discard(size_t size) {
    if (m_pos + size > m_cap)
        VCML_REPORT("attempt to discard beyond end of buffer");

    size_t done = 0;
    while (done < size) {
        size_t off = m_pos % SECTOR_SIZE;
        size_t num = min(SECTOR_SIZE - off, size - done);

        auto it = m_sectors.find(m_pos / SECTOR_SIZE);
        if (it != m_sectors.end() && num == SECTOR_SIZE) {
            delete[] it->second;
            m_sectors.erase(it);
        }

        m_pos += size;
        done += size;
    }
}

void backend_ram::save(ostream& os) {
    for (const auto& sector : m_sectors) {
        os.seekp(sector.first * SECTOR_SIZE);
        os.write((char*)sector.second, SECTOR_SIZE);
        VCML_REPORT_ON(!os, "error saving disk: %s", strerror(errno));
    }
}

void backend_ram::flush() {
    // nothing to do
}

} // namespace block
} // namespace vcml
