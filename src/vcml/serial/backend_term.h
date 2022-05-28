/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_SERIAL_BACKEND_TERM_H
#define VCML_SERIAL_BACKEND_TERM_H

#include <signal.h>
#include <termios.h>
#include <fcntl.h>

#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/aio.h"

#include "vcml/logging/logger.h"
#include "vcml/serial/backend.h"

namespace vcml {
namespace serial {

class backend_term : public backend
{
private:
    mutex m_fifo_mtx;
    queue<u8> m_fifo;

    int m_signal;
    bool m_exit;
    bool m_stopped;

    termios m_termios;
    double m_time;

    sighandler_t m_sigint;
    sighandler_t m_sigstp;

    static backend_term* singleton;
    static void handle_signal(int sig);

    void handle_sigstp(int sig);
    void handle_sigint(int sig);

    void cleanup();

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
