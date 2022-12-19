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

void backend_ram::write(u8 data, size_t count) {
    if (m_pos + count > m_cap)
        VCML_REPORT("attempt to read beyond end of buffer");

    size_t done = 0;
    while (done < count) {
        size_t off = m_pos % SECTOR_SIZE;
        size_t num = min(SECTOR_SIZE - off, count - done);
        u8*& sector = m_sectors[m_pos / SECTOR_SIZE];
        if (!sector && data)
            sector = new u8[SECTOR_SIZE]();

        if (sector)
            memset(sector + off, data, num);

        m_pos += count;
        done += count;
    }
}

void backend_ram::save(ostream& os) {
    for (const auto& sector : m_sectors) {
        os.seekp(sector.first * SECTOR_SIZE);
        os.write((char*)sector.second, SECTOR_SIZE);
        VCML_REPORT_ON(!os, "error saving disk: %s", strerror(errno));
    }
}

} // namespace block
} // namespace vcml
