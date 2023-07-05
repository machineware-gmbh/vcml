#include "vcml/ui/tui.h"

namespace vcml {
namespace ui {

void tui::tuithread() {
    const u64 update_interval = 1000000;
    u64 last_sim = time_to_us(sc_time_stamp());
    u64 last_host = mwr::timestamp_us();

    while (sim_running()) {
        const u64 now_sim = time_to_us(sc_time_stamp());
        const u64 now_host = mwr::timestamp_us();
        const u64 delta = now_host - last_host;
        if (delta >= update_interval) {
            const double rtf = (double)(now_sim - last_sim) / delta;

            const size_t millis = (now_sim % 1000000) / 1000;
            const size_t times = now_sim / 1000000;
            const size_t hours = times / 3600;
            const size_t minutes = (times % 3600) / 60;
            const size_t seconds = times % 60;

            string title = mkstr(
                "+===== time %02zu:%02zu:%02zu.%03zu === delta %lld === rtf "
                "%.2f ",
                hours, minutes, seconds, millis, delta, rtf);
            m_term.get_win().write(title, ncurses::position{ 0, 0 });
            m_term.refresh();
            last_sim = now_sim;
            last_host = now_host;
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(update_interval));
    }
}

tui* tui::tui_instance = nullptr;

tui::tui():
    m_term(),
    m_tui_window(m_term.get_dimensions(), ncurses::position{ 1, 1 }),
    m_tui_thread(&tui::tuithread, this) {
    tui_instance = this;
    mwr::set_thread_name(m_tui_thread, "tui_thread");
}

tui* tui::instance() {
    if (!tui_instance) {
        tui_instance = new tui;
    }
    return tui_instance;
}

ncurses::window& tui::get_tui_window() {
    return m_tui_window;
}

} // namespace ui
} // namespace vcml
