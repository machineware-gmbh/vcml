/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BLOCK_BACKEND_FILE_H
#define VCML_BLOCK_BACKEND_FILE_H

#include "vcml/core/types.h"

#include "vcml/models/block/backend.h"

namespace vcml {
namespace block {

class backend_file : public backend
{
protected:
    string m_path;
    fstream m_stream;
    size_t m_capacity;

public:
    backend_file(const string& path, bool readonly);
    virtual ~backend_file();

    virtual size_t capacity() override;
    virtual size_t pos() override;

    virtual void seek(size_t pos) override;
    virtual void read(u8* buffer, size_t size) override;
    virtual void write(const u8* buffer, size_t size) override;
    virtual void save(ostream& os) override;
    virtual void flush() override;
};

} // namespace block
} // namespace vcml

#endif
