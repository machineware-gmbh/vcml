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

#include <csignal>
#include <sys/ioctl.h>
#include <unistd.h>

#define STSBAR_UPD_INTVL 1000000

vcml::u32 vcml::serial::backend_tui::m_max_cols = 0;

void winsize_sighandler(int sig) {
    VCML_ERROR_ON(sig != SIGWINCH, "Wrong signal received!");
    vcml::serial::backend_tui::update_max_cols();
}

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
        draw_statusbar();
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

void backend_tui::draw_statusbar(const bool now) {
    const u64 now_sim = time_to_us(sc_time_stamp());
    const u64 now_host = mwr::timestamp_us();
    const u64 delta_host = now_host - m_last_time_host;
    if (delta_host >= STSBAR_UPD_INTVL || now) {
        const double rtf = (double)(now_sim - m_last_time_sim) / delta_host;

        const size_t millis = (now_sim % 1000000) / 1000;
        const size_t times = now_sim / 1000000;
        const size_t hours = times / 3600;
        const size_t minutes = (times % 3600) / 60;
        const size_t seconds = times % 60;

        string text = mkstr(
            "+===== time %02zu:%02zu:%02zu.%03zu === "
            "delta %lld === rtf "
            "%.2f ",
            hours, minutes, seconds, millis, sc_delta_count(), rtf);
        text.append(m_max_cols - text.length() - 1, '=');
        text.append("+");

        const string statusbar = mkstr(
            "\x1b[s"     // save cursor position
            "\x1b[?25l"  // hide cursor
            "\x1b[1;1H"  // move cursor to top left
            "%s"         // statusbar text
            "\x1b[u"     // reset cursor position
            "\x1b[?25h", // show cursor
            text.c_str());

        mwr::fd_write(m_fd, statusbar.c_str(), statusbar.length());
        m_last_time_sim = now_sim;
        m_last_time_host = now_host;
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
    m_last_time_sim(time_to_us(sc_time_stamp())),
    m_last_time_host(mwr::timestamp_us()) {
    VCML_REPORT_ON(!isatty(m_fd), "not a terminal");
    capture_stdin();
    mwr::tty_push(m_fd, true);
    mwr::tty_set(m_fd, false, false);

    update_max_cols();
    std::signal(SIGWINCH, winsize_sighandler);

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
    mwr::fd_write(m_fd, &val, sizeof(val));
    if (val == '\n') // re-draw statusbar when screen scrolls
        draw_statusbar(true);
}

backend* backend_tui::create(terminal* term, const string& type) {
    return new backend_tui(term);
}

void backend_tui::update_max_cols() {
    struct winsize tty_size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_size);
    m_max_cols = tty_size.ws_col;
}

} // namespace serial
} // namespace vcml
