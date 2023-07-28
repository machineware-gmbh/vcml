/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_BACKEND_TERM_H
#define VCML_SERIAL_BACKEND_TERM_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"
#include "vcml/models/serial/backend.h"

namespace vcml {
namespace serial {

class backend_term : public backend
{
private:
    int m_fdin;
    int m_fdout;

    atomic<bool> m_exit_requested;
    atomic<bool> m_backend_active;

    thread m_iothread;
    mutable mutex m_mtx;
    queue<u8> m_fifo;

    void iothread();
    void terminate();

public:
    backend_term(terminal* term);
    virtual ~backend_term();

    virtual bool read(u8& val) override;
    virtual void write(u8 val) override;

    static backend* create(terminal* term, const string& type);
};

} // namespace serial
} // namespace vcml

#endif
