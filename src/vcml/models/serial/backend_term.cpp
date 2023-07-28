/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
        if (mwr::fd_peek(m_fdin, 100)) {
            u8 ch;
            if (!mwr::fd_read(m_fdin, &ch, sizeof(ch))) {
                log_warn("eof while reading stdin");
                return; // EOF
            }

            if (ch == CTRL_A) { // ctrl-a
                mwr::fd_read(m_fdin, &ch, sizeof(ch));
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
    m_fdin(STDIN_FILENO),
    m_fdout(STDOUT_FILENO),
    m_exit_requested(false),
    m_backend_active(true),
    m_iothread(),
    m_mtx(),
    m_fifo() {
    capture_stdin();
    VCML_REPORT_ON(!isatty(m_fdin), "not a terminal");
    mwr::tty_push(m_fdin, true);
    mwr::tty_set(m_fdin, false, false);
    m_iothread = thread(&backend_term::iothread, this);
    mwr::set_thread_name(m_iothread, "term_iothread");
}

backend_term::~backend_term() {
    m_backend_active = false;
    if (m_iothread.joinable())
        m_iothread.join();

    mwr::tty_pop(m_fdin);
    release_stdin();
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
    mwr::fd_write(m_fdout, &val, sizeof(val));
}

backend* backend_term::create(terminal* term, const string& type) {
    return new backend_term(term);
}

} // namespace serial
} // namespace vcml
