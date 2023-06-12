/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BLOCK_DISK_H
#define VCML_BLOCK_DISK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/properties/property.h"
#include "vcml/models/block/backend.h"

namespace vcml {
namespace block {

class disk : public module
{
private:
    backend* m_backend;

    bool cmd_show_stats(const vector<string>& args, ostream& os);
    bool cmd_save_image(const vector<string>& args, ostream& os);

public:
    struct stats {
        size_t num_bytes_read;
        size_t num_bytes_written;
        size_t num_seek_req;
        size_t num_read_req;
        size_t num_write_req;
        size_t num_flush_req;
        size_t num_discard_req;
        size_t num_req;
        size_t num_seek_err;
        size_t num_read_err;
        size_t num_write_err;
        size_t num_flush_err;
        size_t num_discard_err;
        size_t num_err;
    } stats;

    property<string> image;
    property<string> serial;
    property<bool> readonly;

    bool has_backing() const { return m_backend != nullptr; }

    disk(const sc_module_name& name, const string& img = "",
         bool readonly = false);
    virtual ~disk();
    VCML_KIND(block::drive);

    size_t capacity();
    size_t pos();
    size_t remaining();

    bool seek(size_t pos);
    bool read(u8* buffer, size_t size);
    bool write(const u8* buffer, size_t size);
    bool wzero(size_t size, bool may_unmap = true);
    bool discard(size_t size);
    bool flush();
};

} // namespace block
} // namespace vcml

#endif
