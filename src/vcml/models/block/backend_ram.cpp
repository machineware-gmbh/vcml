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
    backend("ramdisk", readonly), m_capacity(cap), m_buf(), m_pos(), m_end() {
    m_buf = m_pos = new u8[m_capacity];
    m_end = m_buf + m_capacity;
}

backend_ram::~backend_ram() {
    if (m_buf)
        delete[] m_buf;
}

size_t backend_ram::capacity() {
    return m_capacity;
}

size_t backend_ram::pos() {
    return m_pos - m_buf;
}

void backend_ram::seek(size_t pos) {
    VCML_REPORT_ON(pos > capacity(), "attempt to seek beyond end of buffer");
    m_pos = m_buf + pos;
}

void backend_ram::read(vector<u8>& buffer) {
    if (m_pos + buffer.size() > m_end)
        VCML_REPORT("attempt to read beyond end of buffer");
    memcpy(buffer.data(), m_pos, buffer.size());
}

void backend_ram::write(const vector<u8>& buffer) {
    if (m_pos + buffer.size() > m_end)
        VCML_REPORT("attempt to write beyond end of buffer");
    memcpy(m_pos, buffer.data(), buffer.size());
}

void backend_ram::write(u8 data, size_t count) {
    if (m_pos + count > m_end)
        VCML_REPORT("attempt to read beyond end of buffer");
    memset(m_pos, data, count);
}

void backend_ram::save(ostream& os) {
    os.write((char*)m_buf, m_capacity);
    VCML_REPORT_ON(!os.good(), "I/O error");
}

} // namespace block
} // namespace vcml
