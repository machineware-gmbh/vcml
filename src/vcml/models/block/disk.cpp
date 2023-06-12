/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/block/disk.h"

namespace vcml {
namespace block {

bool disk::cmd_show_stats(const vector<string>& args, ostream& os) {
    os << "bytes read       " << stats.num_bytes_read << std::endl;
    os << "bytes written    " << stats.num_bytes_written << std::endl;
    os << "seek requests    " << stats.num_seek_req << std::endl;
    os << "read requests    " << stats.num_read_req << std::endl;
    os << "write requests   " << stats.num_write_req << std::endl;
    os << "flush requests   " << stats.num_write_req << std::endl;
    os << "discard requests " << stats.num_discard_req << std::endl;
    os << "total requests   " << stats.num_req << std::endl;
    os << "seek errors      " << stats.num_seek_err << std::endl;
    os << "read errors      " << stats.num_read_err << std::endl;
    os << "write errors     " << stats.num_write_err << std::endl;
    os << "flush errors     " << stats.num_flush_err << std::endl;
    os << "discard errors   " << stats.num_discard_err << std::endl;
    os << "total errors     " << stats.num_err << std::endl;
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

static string default_serial() {
    static size_t n = 0;
    return mkstr("vcml-disk-%zu", n++);
}

disk::disk(const sc_module_name& nm, const string& img, bool ro):
    module(nm),
    m_backend(nullptr),
    stats(),
    image("image", img),
    serial("serial", default_serial()),
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

bool disk::read(u8* buffer, size_t size) {
    stats.num_read_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            m_backend->read(buffer, size);
            stats.num_bytes_read += size;
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_read_err++;
    stats.num_err++;
    return false;
}

bool disk::write(const u8* buffer, size_t size) {
    stats.num_write_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            if (!m_backend->readonly()) {
                m_backend->write(buffer, size);
                stats.num_bytes_written += size;
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

bool disk::wzero(size_t size, bool may_unmap) {
    stats.num_write_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            if (!m_backend->readonly()) {
                m_backend->wzero(size, may_unmap);
                stats.num_bytes_written += size;
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

bool disk::discard(size_t size) {
    stats.num_discard_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            m_backend->discard(size);
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_discard_err++;
    stats.num_err++;
    return false;
}

bool disk::flush() {
    stats.num_flush_req++;
    stats.num_req++;

    if (m_backend) {
        try {
            m_backend->flush();
            return true;
        } catch (std::exception& ex) {
            log.warn(ex);
        }
    }

    stats.num_flush_err++;
    stats.num_err++;
    return false;
}

} // namespace block
} // namespace vcml
