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

static size_t max_cols = 80;

#ifdef MWR_LINUX
#include <sys/ioctl.h>
#include <unistd.h>

static void update_window_size(int sig) {
    struct winsize tty_size;
    ioctl(mwr::STDOUT_FDNO, TIOCGWINSZ, &tty_size);
    max_cols = tty_size.ws_col;
}

#endif

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
    mwr::set_thread_name("tui_iothread");
    while (m_backend_active && sim_running()) {
        u64 now_host = mwr::timestamp_us();
        u64 now_sim = time_to_us(sc_time_stamp());
        if (now_host > m_time_host) {
            m_rtf = (double)(now_sim - m_time_sim) / (now_host - m_time_host);
            m_time_host = now_host;
            m_time_sim = now_sim;
        }

        m_mtx.lock();
        draw_statusbar();
        m_mtx.unlock();

        if (mwr::fd_peek(m_fdin, 500)) {
            u8 ch;
            if (mwr::fd_read(m_fdin, &ch, sizeof(ch)) == 0)
                continue; // EOF?

            if (ch == CTRL_A) { // ctrl-a
                mwr::fd_read(m_fdin, &ch, sizeof(ch));
                if (ch == 'x' || ch == 'X' || ch == CTRL_X) {
                    terminate();
                    continue;
                }

                if (ch == 'a')
                    ch = CTRL_A;
            }

            m_mtx.lock();
            m_fifo.push(ch);
            m_mtx.unlock();
            m_term->notify(this);
        }
    }
}

void backend_tui::draw_statusbar() {
    if (!sc_core::sc_start_of_simulation_invoked())
        return;

    const u64 now = time_to_us(sc_time_stamp());
    const size_t millis = (now % 1000000) / 1000;
    const size_t times = now / 1000000;
    const size_t hours = times / 3600;
    const size_t minutes = (times % 3600) / 60;
    const size_t seconds = times % 60;

    string text = mkstr(
        " time %02zu:%02zu:%02zu.%03zu   delta %lld   rtf %.2f", hours,
        minutes, seconds, millis, sc_delta_count(), m_rtf);
    if (text.length() < max_cols)
        text.append(max_cols - text.length(), ' ');
    else
        text = text.substr(0, max_cols);

    const string statusbar = mkstr(
        "\n"      // begin status bar
        "\x1b[7m" // invert colors
        "%s"      // print status bar
        "\x1b[0m" // restore colors
        "\x1b[F"  // return to previous line
        "\x1b[K"  // clear line
        "%s",     // print line buffer
        text.c_str(), m_linebuf.c_str());

    mwr::fd_write(m_fdout, statusbar.c_str(), statusbar.length());
}

backend_tui::backend_tui(terminal* term):
    backend(term, "term"),
    m_fdin(STDIN_FDNO),
    m_fdout(STDOUT_FDNO),
    m_exit_requested(false),
    m_backend_active(true),
    m_iothread(),
    m_mtx(),
    m_fifo(),
    m_time_sim(time_to_us(sc_time_stamp())),
    m_time_host(mwr::timestamp_us()),
    m_rtf(),
    m_linebuf() {
    capture_stdin();

    if (mwr::is_tty(m_fdin)) {
        mwr::tty_push(m_fdin, true);
        mwr::tty_setup_vt100(m_fdin);
    }

#ifdef MWR_LINUX
    update_window_size(0);
    std::signal(SIGWINCH, update_window_size);
#endif

    m_iothread = thread(&backend_tui::iothread, this);
}

backend_tui::~backend_tui() {
    m_backend_active = false;
    if (m_iothread.joinable())
        m_iothread.join();

    if (mwr::is_tty(m_fdin))
        mwr::tty_pop(m_fdin);

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
    lock_guard<mutex> lock(m_mtx);
    if (val == '\n' || m_linebuf.length() >= max_cols) {
        string line = mkstr("\r\x1b[K%s\n", m_linebuf.c_str());
        mwr::fd_write(m_fdout, line.data(), line.size());
        m_linebuf.clear();
    } else {
        m_linebuf.push_back(val);
    }

    draw_statusbar();
}

backend* backend_tui::create(terminal* term, const string& type) {
    return new backend_tui(term);
}

} // namespace serial
} // namespace vcml
