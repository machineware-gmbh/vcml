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

#include "vcml/models/block/backend_file.h"

namespace vcml {
namespace block {

backend_file::backend_file(const string& path, bool readonly):
    backend("file", readonly), m_path(path), m_stream(), m_capacity() {
    auto flags = std::ios_base::binary | std::ios_base::in;
    if (!readonly)
        flags |= std::ios_base::out;

    m_stream.open(m_path.c_str(), flags);
    VCML_REPORT_ON(!m_stream, "error opening %s", m_path.c_str());

    m_stream.seekg(0, std::ios_base::end);
    m_capacity = m_stream.tellg();
    m_stream.seekg(0, std::ios_base::beg);
}

backend_file::~backend_file() {
    // nothing to do
}

size_t backend_file::capacity() {
    return m_capacity;
}

size_t backend_file::pos() {
    return m_stream.tellg();
}

void backend_file::seek(size_t pos) {
    VCML_REPORT_ON(pos > capacity(), "attempt to seek beyond end of buffer");
    m_stream.seekg(pos);
    m_stream.seekp(pos);
}

void backend_file::read(u8* buffer, size_t size) {
    VCML_REPORT_ON(size > remaining(), "reading beyond end of file");
    m_stream.read((char*)buffer, size);
    VCML_REPORT_ON(!m_stream, "error reading: %s", strerror(errno));
}

void backend_file::write(const u8* buffer, size_t size) {
    VCML_REPORT_ON(size > remaining(), "writing beyond end of file");
    m_stream.write((char*)buffer, size);
    VCML_REPORT_ON(!m_stream, "error writing: %s", strerror(errno));
}

void backend_file::save(ostream& os) {
    os << m_stream.rdbuf();
}

void backend_file::flush() {
    m_stream << std::flush;
}

} // namespace block
} // namespace vcml
