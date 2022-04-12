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

#include "vcml/serial/backend_term.h"
#include "vcml/debugging/suspender.h"

namespace vcml {
namespace serial {

backend_term* backend_term::singleton = nullptr;

void backend_term::handle_signal(int sig) {
    if (singleton) {
        if (sig == SIGINT) /* interrupt (ANSI) */
            return singleton->handle_sigint(sig);
        if (sig == SIGTSTP) /* keyboard stop (POSIX) */
            singleton->handle_sigstp(sig);
    }

    // this should not happen, but just to be safe...
    signal(sig, SIG_DFL);
    raise(sig);
}

void backend_term::handle_sigstp(int sig) {
    m_signal = m_termios.c_cc[VSUSP];
    if ((m_sigstp != SIG_DFL) && (m_sigstp != SIG_IGN))
        (*m_sigstp)(sig);
}

void backend_term::handle_sigint(int sig) {
    double now = realtime();
    if ((now - m_time) < 1.0) {
        tcsetattr(STDIN_FILENO, TCSANOW, &m_termios);
        if (m_stopped || m_exit || !sc_core::sc_is_running()) {
            cleanup();
            exit(EXIT_SUCCESS);
        } else {
            m_stopped = true;
            debugging::suspender::quit();
        }
    }

    m_time   = now;
    m_signal = m_termios.c_cc[VINTR];
    if ((m_sigint != SIG_DFL) && (m_sigint != SIG_IGN))
        (*m_sigint)(sig);
}

void backend_term::cleanup() {
    signal(SIGINT, m_sigint);
    signal(SIGTSTP, m_sigstp);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &m_termios) == -1)
        log_error("failed to reset terminal");

    aio_cancel(STDIN_FILENO);
}

backend_term::backend_term(const string& port):
    backend(port),
    m_fifo_mtx(),
    m_fifo(),
    m_signal(0),
    m_exit(false),
    m_stopped(false),
    m_termios(),
    m_time(realtime()),
    m_sigint(),
    m_sigstp() {
    VCML_REPORT_ON(singleton, "multiple terminal backends requested");
    singleton = this;

    m_type = "term";

    if (!isatty(STDIN_FILENO))
        VCML_REPORT("not a terminal");

    if (tcgetattr(STDIN_FILENO, &m_termios) == -1)
        VCML_REPORT("failed to get terminal attributes");

    termios attr = m_termios;
    attr.c_lflag &= ~(ICANON | ECHO);
    attr.c_cc[VMIN]  = 1;
    attr.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr) == -1)
        VCML_REPORT("failed to set terminal attributes");

    m_sigint = signal(SIGINT, &backend_term::handle_signal);
    m_sigstp = signal(SIGTSTP, &backend_term::handle_signal);

    aio_notify(STDIN_FILENO, [&](int fd) -> void {
        u8 data;
        if (fd_read(fd, &data, sizeof(data)) == sizeof(data)) {
            lock_guard<mutex> guard(m_fifo_mtx);
            m_fifo.push(data);
        }
    });
}

backend_term::~backend_term() {
    cleanup();
    singleton = nullptr;
}

bool backend_term::read(u8& value) {
    if (m_signal != 0) {
        value    = (u8)m_signal;
        m_signal = 0;
        return true;
    }

    lock_guard<mutex> guard(m_fifo_mtx);
    if (m_fifo.empty())
        return false;

    value = m_fifo.front();
    m_fifo.pop();
    return true;
}

void backend_term::write(u8 val) {
    fd_write(STDOUT_FILENO, &val, sizeof(val));
}

backend* backend_term::create(const string& port, const string& type) {
    return new backend_term(port);
}

} // namespace serial
} // namespace vcml
