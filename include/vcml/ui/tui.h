#ifndef VCML_UI_TUI_H
#define VCML_UI_TUI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/ncurses.h"

namespace vcml {
namespace ui {
class tui
{
private:
    ncurses::terminal m_term;
    ncurses::window m_tui_window;

    thread m_tui_thread;

    static tui* tui_instance;

    void tuithread();

    tui();
    ~tui() = delete;

public:
    tui(const tui&) = delete;
    tui(tui&&) = delete;
    tui& operator=(const tui&) = delete;
    tui& operator=(tui&&) = delete;

    static tui* instance();

    ncurses::window& get_tui_window();
};
} // namespace ui
} // namespace vcml

#endif
