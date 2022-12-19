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

#include "vcml/models/block/disk.h"

namespace vcml {
namespace block {

bool disk::cmd_show_stats(const vector<string>& args, ostream& os) {
    os << "bytes read     " << stats.num_bytes_read << std::endl;
    os << "bytes written  " << stats.num_bytes_written << std::endl;
    os << "seek requests  " << stats.num_seek_req << std::endl;
    os << "read requests  " << stats.num_read_req << std::endl;
    os << "write requests " << stats.num_write_req << std::endl;
    os << "total requests " << stats.num_req << std::endl;
    os << "seek errors    " << stats.num_seek_err << std::endl;
    os << "read errors    " << stats.num_read_err << std::endl;
    os << "write errors   " << stats.num_write_err << std::endl;
    os << "total errors   " << stats.num_err << std::endl;
    return true;
}

bool disk::cmd_save_image(const vector<string>& args, ostream& os) {
    const string& file = args[1];
    ofstream stream(file.c_str(), ofstream::binary);
    if (!stream.good()) {
        os << mkstr("cannot open '%s'", file.c_str());
        return false;
    }

    try {
        m_backend->save(stream);
        return true;
    } catch (std::exception& ex) {
        os << "error saving image: " << ex.what();
        return false;
    }
}

disk::disk(const sc_module_name& nm, const string& img, bool ro):
    module(nm),
    m_backend(nullptr),
    stats(),
    image("image", img),
    readonly("readonly", ro) {
    try {
        m_backend = backend::create(image, readonly);
        readonly = !m_backend || m_backend->readonly();
    } catch (std::exception& ex) {
        log_warn("%s", ex.what());
    }
}

disk::~disk() {
    if (m_backend)
        delete m_backend;
}

size_t disk::capacity() {
    return m_backend ? m_backend->capacity() : 0;
}

size_t disk::pos() {
    return m_backend ? m_backend->pos() : 0;
}

size_t disk::remaining() {
    return m_backend ? m_backend->remaining() : 0;
}

bool disk::seek(size_t pos) {
    stats.num_seek_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            m_backend->seek(pos);
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_seek_err++;
    stats.num_err++;
    return false;
}

bool disk::read(vector<u8>& buffer) {
    stats.num_read_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            m_backend->read(buffer);
            stats.num_bytes_read += buffer.size();
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_read_err++;
    stats.num_err++;
    return false;
}

bool disk::write(const vector<u8>& buffer) {
    stats.num_write_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            if (!m_backend->readonly()) {
                m_backend->write(buffer);
                stats.num_bytes_written += buffer.size();
            }
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_write_err++;
    stats.num_err++;
    return false;
}

bool disk::write(u8 data, size_t count) {
    stats.num_write_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            if (!m_backend->readonly()) {
                m_backend->write(data, count);
                stats.num_bytes_written += count;
            }
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_write_err++;
    stats.num_err++;
    return false;
}

} // namespace block
} // namespace vcml
