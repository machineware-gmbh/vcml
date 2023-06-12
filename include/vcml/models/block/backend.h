/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BLOCK_BACKEND_H
#define VCML_BLOCK_BACKEND_H

#include "vcml/core/types.h"

namespace vcml {
namespace block {

class backend
{
protected:
    string m_type;
    bool m_readonly;

public:
    const char* type() const { return m_type.c_str(); }
    bool readonly() const { return m_readonly; }

    backend(const string& type, bool readonly);
    virtual ~backend() = default;

    backend() = delete;
    backend(const backend&) = delete;
    backend(backend&&) = default;

    virtual size_t capacity() = 0;
    virtual size_t pos() = 0;
    virtual size_t remaining();

    virtual void seek(size_t pos) = 0;
    virtual void read(u8* buffer, size_t size) = 0;
    virtual void write(const u8* buffer, size_t size) = 0;
    virtual void save(ostream& os) = 0;

    virtual void wzero(size_t size, bool may_unmap);
    virtual void discard(size_t size);
    virtual void flush();

    static backend* create(const string& image, bool readonly);
};

} // namespace block
} // namespace vcml

#endif
