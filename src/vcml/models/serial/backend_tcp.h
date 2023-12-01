/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_BACKEND_TCP_H
#define VCML_SERIAL_BACKEND_TCP_H

#include "vcml/core/types.h"
#include "vcml/logging/logger.h"
#include "vcml/models/serial/backend.h"

namespace vcml {
namespace serial {

class backend_tcp : public backend
{
private:
    mwr::socket m_socket;

    thread m_thread;
    mutex m_mtx;
    queue<u8> m_fifo;
    atomic<bool> m_running;

    void iothread();
    void receive();

public:
    u16 port() const { return m_socket.port(); }

    backend_tcp(terminal* term, u16 port);
    virtual ~backend_tcp();

    virtual bool read(u8& val) override;
    virtual void write(u8 val) override;

    static backend* create(terminal* term, const string& type);
};

} // namespace serial
} // namespace vcml

#endif
