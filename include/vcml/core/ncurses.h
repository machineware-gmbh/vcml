#ifndef VCML_CORE_NCURSES_H
#define VCML_CORE_NCURSES_H

#include "vcml/core/types.h"

#include <ncurses.h>

#define COLPAIR(FG, BG) (FG << 4 | BG)
#define FGCOL(FG)       COLPAIR(FG, ncurses::colors::BLACK)

namespace vcml {
namespace ncurses {

struct dimensions {
    int colums;
    int lines;
};

struct position {
    int x;
    int y;
};

enum colors { BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE };

enum ansi_escape_mode { NONE, CSI };

class window
{
private:
    std::unique_ptr<::WINDOW> m_win;

    bool m_escape_enabled;
    ansi_escape_mode m_curr_mode;
    queue<u8> m_escape_args;

    mutex m_mtx;

public:
    window() = delete;
    window(WINDOW* win);
    window(const dimensions& dim, const position& pos);
    ~window();

    void draw_border(const char ls, const char rs, const char ts,
                     const char bs, const char tl, const char tr,
                     const char bl, const char br);

    void write(const char val);
    void write(const std::string& str);
    void write(const std::string& str, const position& pos);
    void write(const std::string& str, const int color);

    void set_scrolling(const bool enable);
    void set_timeout(const int timeout_ms = 100);

    position get_curs_pos() const;
    void move_curs(const position& pos);

    void refresh();

    i8 get_char() const;

    void clear_to_bottom();
    void clear_screen();

    void newline();
};

class terminal
{
private:
    ncurses::window m_main_win;

public:
    terminal();
    ~terminal();

    dimensions get_dimensions();
    ncurses::window& get_win() { return m_main_win; }

    void refresh();
};
} // namespace ncurses
} // namespace vcml

#endif
