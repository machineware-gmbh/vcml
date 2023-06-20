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
#include "vcml/models/serial/backend_tui.h"

#include "vcml/debugging/suspender.h"

namespace vcml {
namespace serial {

enum keys : u8 {
    CTRL_A = 0x1,
    CTRL_C = 0x3,
    CTRL_X = 0x18,
};

void backend_tui::terminate() {
    if (m_exit_requested || !sim_running()) {
        log_info("forced exit");
        exit(EXIT_SUCCESS);
    }

    m_exit_requested = true;
    debugging::suspender::quit();
}

void backend_tui::iothread() {
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

backend_tui::backend_tui(terminal* term):
    backend(term, "term"),
    m_fd(STDIN_FILENO),
    m_exit_requested(false),
    m_backend_active(true),
    m_iothread(),
    m_mtx(),
    m_fifo(),
    log("terminal") {
    VCML_REPORT_ON(!isatty(m_fd), "not a terminal");
    capture_stdin();
    mwr::tty_push(m_fd, true);
    mwr::tty_set(m_fd, false, false);
    m_iothread = thread(&backend_tui::iothread, this);
    mwr::set_thread_name(m_iothread, "tui_iothread");
}

backend_tui::~backend_tui() {
    m_backend_active = false;
    if (m_iothread.joinable())
        m_iothread.join();

    mwr::tty_pop(m_fd);
    release_stdin();
}

bool backend_tui::read(u8& value) {
    lock_guard<mutex> lock(m_mtx);
    if (m_fifo.empty())
        return false;

    value = m_fifo.front();
    m_fifo.pop();
    return true;
}

void backend_tui::write(u8 val) {
    // ToDo: draw a TUI frame with the new character appended
    mwr::fd_write(m_fd, &val, sizeof(val));
}

backend* backend_tui::create(terminal* term, const string& type) {
    return new backend_tui(term);
}

} // namespace serial
} // namespace vcml
