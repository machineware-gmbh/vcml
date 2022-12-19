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
