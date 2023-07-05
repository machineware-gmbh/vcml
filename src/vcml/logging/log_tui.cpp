/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/log_tui.h"

namespace vcml {

log_tui::log_tui(bool use_colors): publisher(), m_colors(use_colors) {
    // nothing to do
}

void log_tui::publish(const mwr::logmsg& msg) {
    ncurses::window& win = ui::tui::instance()->get_tui_window();

    stringstream ss;
    ss << msg << std::endl;
    if (m_colors) {
        win.write(ss.str(), COLORS[msg.level]);
    } else {
        win.write(ss.str());
    }
    win.refresh();
}

const int log_tui::COLORS[mwr::log_level::NUM_LOG_LEVELS] = {
    /* [LOG_ERROR] = */ FGCOL(ncurses::colors::RED),
    /* [LOG_WARN]  = */ FGCOL(ncurses::colors::YELLOW),
    /* [LOG_INFO]  = */ FGCOL(ncurses::colors::GREEN),
    /* [LOG_DEBUG] = */ FGCOL(ncurses::colors::BLUE),
};

} // namespace vcml
