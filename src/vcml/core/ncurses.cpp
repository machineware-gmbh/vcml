#include "vcml/core/ncurses.h"

#define ASCII_TO_INT(x) ((int)(x - 0x30))

namespace vcml {
namespace ncurses {

int get_escape_arg(queue<u8>& queue) {
    int tmp = 0;
    while (queue.front() != ';' && !queue.empty()) {
        tmp = tmp * 10 + ASCII_TO_INT(queue.front());
        queue.pop();
    }
    if (!queue.empty())
        queue.pop();
    return tmp;
}

window::window(::WINDOW* win): m_win(std::unique_ptr<::WINDOW>(win)), m_mtx() {
    if (!m_win) {
        printf("Could not initialize TUI!");
        std::abort();
    }
}

window::window(const dimensions& dim, const position& pos):
    m_win(::newwin(dim.lines, dim.colums, pos.y, pos.x)), m_mtx() {
    set_scrolling(true);
    set_timeout();
}

window::~window() {
    ::delwin(m_win.get());
    m_win.release();
}

void window::draw_border(const char ls, const char rs, const char ts,
                         const char bs, const char tl, const char tr,
                         const char bl, const char br) {
    ::wborder(m_win.get(), ls, rs, ts, bs, tl, tr, bl, br);
}

void window::write(const char val) {
    lock_guard<mutex> lock(m_mtx);
    if (m_escape_enabled) {
        if (m_curr_mode != ansi_escape_mode::NONE) {
            if (val <= 0x40 || val >= 0x7e) {
                m_escape_args.push(val);
            } else { // parse escape
                const ncurses::position tmp = get_curs_pos();
                switch (val) {
                case 'A': // CUU
                    move_curs(
                        { .x = tmp.x,
                          .y = tmp.y == 0
                                   ? 0
                                   : tmp.y - get_escape_arg(m_escape_args) });
                    break;
                case 'B': // CUD
                    move_curs(
                        { .x = tmp.x,
                          .y = tmp.y == m_win->_maxy
                                   ? tmp.y
                                   : tmp.y + get_escape_arg(m_escape_args) });
                    break;
                case 'C': // CUF
                    move_curs(
                        { .x = tmp.x == m_win->_maxx
                                   ? tmp.x
                                   : tmp.x + get_escape_arg(m_escape_args),
                          .y = tmp.y });
                    break;
                case 'D': // CUB
                    move_curs({ .x = tmp.x == 0 ? 0
                                                : tmp.x - get_escape_arg(
                                                              m_escape_args),
                                .y = tmp.y });
                    break;
                case 'H': // CUP
                {
                    const int row = get_escape_arg(m_escape_args);
                    const int col = get_escape_arg(m_escape_args);
                    move_curs({ .x = col, .y = row });
                } break;
                case 'J': // ED
                    switch (get_escape_arg(m_escape_args)) {
                    case 0:
                        clear_to_bottom();
                        break;
                    case 2:
                    case 3: // TODO: impl scrollback buffer clearing
                        clear_screen();
                        break;
                    default: // TODO: implement other args
                        break;
                    }
                case 'm': // SGR TODO: implement
                    break;
                }

                m_escape_enabled = false;
                m_escape_args = queue<u8>();
                m_curr_mode = ansi_escape_mode::NONE;
            }
        } else {
            switch (val) {
            case '[': // CSI
                m_curr_mode = ansi_escape_mode::CSI;
                break;
            default:
                break;
            }
        }
    } else {
        switch (val) {
        case '\a':
            return; // ignore
        case '\e':
            m_escape_enabled = true;
            return;
        case '\n':
            // custom newline routine
            newline();
            break;
        default:
            ::waddch(m_win.get(), val);
            break;
        }
    }
}

void window::write(const std::string& str) {
    lock_guard<mutex> lock(m_mtx);
    ::waddnstr(m_win.get(), str.c_str(), str.length());
}

void window::write(const std::string& str, const position& pos) {
    lock_guard<mutex> lock(m_mtx);
    ::wmove(m_win.get(), pos.y, pos.x);
    ::waddnstr(m_win.get(), str.c_str(), str.length());
}

void window::write(const std::string& str, const int color) {
    lock_guard<mutex> lock(m_mtx);
    ::wcolor_set(m_win.get(), color, 0);
    ::waddnstr(m_win.get(), str.c_str(), str.length());
    ::wcolor_set(m_win.get(), FGCOL(colors::WHITE), 0);
}

void window::set_scrolling(const bool enable) {
    ::scrollok(m_win.get(), enable);
}

void window::set_timeout(const int timeout_ms) {
    ::wtimeout(m_win.get(), timeout_ms);
}

position window::get_curs_pos() const {
    return position{ m_win->_curx, m_win->_cury };
}

void window::move_curs(const position& pos) {
    ::wmove(m_win.get(), pos.y, pos.x);
}

void window::refresh() {
    ::wrefresh(m_win.get());
}

i8 window::get_char() const {
    return ::wgetch(m_win.get());
}

void window::clear_to_bottom() {
    ::wclrtobot(m_win.get());
}

void window::clear_screen() {
    ::wclear(m_win.get());
}

void window::newline() {
    if (m_win->_cury == m_win->_maxy) {
        ::wscrl(m_win.get(), 1);
    }
    move_curs({ .x = 0, .y = m_win->_cury + 1 });
    ::wclrtoeol(m_win.get());
}

void cleanup_tui() {
    ::endwin();
}

terminal::terminal(): m_main_win(initscr()) {
    ::raw();
    ::noecho();
    atexit(&cleanup_tui);

    if (::has_colors()) {
        ::start_color();
        for (int fg = 0; fg < 8; fg++) {
            for (int bg = 0; bg < 8; bg++) {
                ::init_pair(COLPAIR(fg, bg), fg, bg);
            }
        }
    }

    m_main_win.draw_border('|', '|', '=', '-', '+', '+', '+', '+');

    m_main_win.refresh();
}

terminal::~terminal() {
    // nothing to do
}

dimensions terminal::get_dimensions() {
    return { ::COLS - 2, ::LINES - 3 };
}

void terminal::refresh() {
    ::refresh();
}

} // namespace ncurses
} // namespace vcml
