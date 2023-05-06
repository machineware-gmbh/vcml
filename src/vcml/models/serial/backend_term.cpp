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

#include "vcml/models/serial/terminal.h"
#include "vcml/models/serial/backend_term.h"

#include "vcml/debugging/suspender.h"

namespace vcml {
namespace serial {

enum keys : u8 {
    CTRL_A = 0x1,
    CTRL_C = 0x3,
    CTRL_X = 0x18,
};

static terminal* g_owner = nullptr;

void backend_term::terminate() {
    if (m_exit_requested || !sim_running()) {
        log_info("forced exit");
        exit(EXIT_SUCCESS);
    }

    m_exit_requested = true;
    debugging::suspender::quit();
}

void backend_term::iothread() {
    while (m_backend_active && sim_running()) {
        if (mwr::fd_peek(m_fd, 100)) {
            u8 ch;
            if (!mwr::fd_read(m_fd, &ch, sizeof(ch))) {
                log_warn("eof while reading stdin");
                return; // EOF
            }

            if (ch == CTRL_A) { // ctrl-a
                mwr::fd_read(m_fd, &ch, sizeof(ch));
                if (ch == 'x' || ch == 'X' || ch == CTRL_X) {
                    terminate();
                    continue;
                }

                if (ch == 'a')
                    ch = CTRL_A;
            }

            lock_guard<mutex> lock(m_mtx);
            m_fifo.push(ch);
        }
    }
}

backend_term::backend_term(terminal* term):
    backend(term, "term"),
    m_fd(STDIN_FILENO),
    m_exit_requested(false),
    m_backend_active(true),
    m_iothread(),
    m_mtx(),
    m_fifo(),
    log("terminal") {
    VCML_REPORT_ON(g_owner, "stdin already used by %s", g_owner->name());
    VCML_REPORT_ON(!isatty(m_fd), "not a terminal");
    g_owner = term;
    mwr::tty_push(m_fd, true);
    mwr::tty_set(m_fd, false, false);
    m_iothread = thread(&backend_term::iothread, this);
    mwr::set_thread_name(m_iothread, "term_iothread");
}

backend_term::~backend_term() {
    m_backend_active = false;
    if (m_iothread.joinable())
        m_iothread.join();

    mwr::tty_pop(m_fd);
    g_owner = nullptr;
}

bool backend_term::read(u8& value) {
    lock_guard<mutex> lock(m_mtx);
    if (m_fifo.empty())
        return false;

    value = m_fifo.front();
    m_fifo.pop();
    return true;
}

void backend_term::write(u8 val) {
    mwr::fd_write(m_fd, &val, sizeof(val));
}

backend* backend_term::create(terminal* term, const string& type) {
    return new backend_term(term);
}

} // namespace serial
} // namespace vcml
