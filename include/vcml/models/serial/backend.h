/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_BACKEND_H
#define VCML_SERIAL_BACKEND_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"

namespace vcml {
namespace serial {

class terminal;

class backend
{
protected:
    terminal* m_term;
    string m_type;

public:
    logger& log;

    backend(terminal* term, const string& type);
    virtual ~backend();

    backend() = delete;
    backend(const backend&) = delete;
    backend(backend&&) = default;

    terminal* term() const { return m_term; }

    const char* type() const { return m_type.c_str(); }

    virtual bool read(u8& val) = 0;
    virtual void write(u8 val) = 0;

    void capture_stdin();
    void release_stdin();

    static backend* create(terminal* term, const string& type);
};

} // namespace serial
} // namespace vcml

#endif
