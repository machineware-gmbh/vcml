/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/i2c/ssd1306.h"

namespace vcml {
namespace i2c {

constexpr double BANK0_PULSE_WIDTH = 50.0;
constexpr double OSC_FREQ = 370000.0;
constexpr u16 SCROLL_STEPS[8] = { 5, 64, 128, 256, 3, 4, 25, 2 };

ssd1306::ssd1306(const sc_core::sc_module_name& nm):
    component(nm),
    i2c_host(),
    alternate_address("alternate_address", false),
    rotated("rotated", true),
    color_off("color_off", 0x000000u),
    color_on("color_on", 0xffffffu),
    i2c_in("i2c_in"),
    m_display("display") {
    i2c_in.set_address(alternate_address ? 0x3du : 0x3cu);
    clk.stub();

    SC_HAS_PROCESS(ssd1306);
    SC_THREAD(scroll_thread);

    reset();
}

void ssd1306::end_of_elaboration() {
    m_display.setup(ui::videomode(PIXEL_FORMAT, SCREEN_WIDTH, SCREEN_HEIGHT),
                    m_video);
}

bool ssd1306::is_display_on() const {
    return m_display_on;
}

bool ssd1306::is_inverse() const {
    return m_inverse;
}

u8 ssd1306::get_contrast() const {
    return m_contrast;
}

bool ssd1306::read_pixel(u32 x, u32 y) const {
    return m_display.read_pixel(x, y) != color_off;
}

bool ssd1306::is_scrolling() const {
    return m_scroll_on;
}

sc_core::sc_time ssd1306::frame_period() const {
    double divide_ratio = m_divide_ratio + 1;
    double clocks_per_row = m_phase1_period + m_phase2_period +
                            BANK0_PULSE_WIDTH;
    double freq = OSC_FREQ / (divide_ratio * clocks_per_row * m_mux_ratio);
    return sc_core::sc_time(1.0 / freq, sc_core::SC_SEC);
}

sc_core::sc_time ssd1306::scroll_step_period() const {
    return frame_period() * m_scroll_frames;
}

void ssd1306::reset() {
    memset(m_gddram, 0, sizeof(m_gddram));
    fill_all_pixels(color_off.get());

    m_await_control = true;
    m_stream_data = false;
    m_data_mode = false;

    m_cmd_opcode = 0;
    m_cmd_args_needed = 0;
    m_cmd_args_have = 0;

    m_addr_mode = MODE_PAGE;
    m_col = 0;
    m_page = 0;
    m_col_start = 0;
    m_col_end = SCREEN_WIDTH - 1;
    m_page_start = 0;
    m_page_end = NUM_PAGES - 1;
    m_page_mode_col_start = 0;
    m_contrast = 0x7f;
    m_divide_ratio = 0;
    m_osc_freq = 8;
    m_phase1_period = 2;
    m_phase2_period = 2;
    m_deselect_level = 2;
    m_vertical_shift = 0;
    m_mux_ratio = 64;

    m_display_on = false;
    m_inverse = false;
    m_entire_display_on = false;
    m_segment_remap = false;
    m_com_scan_remap = false;

    m_scroll_on = false;
    m_scroll_left = false;
    m_scroll_vertical = false;
    m_scroll_page_start = 0;
    m_scroll_page_end = NUM_PAGES - 1;
    m_scroll_frames = SCROLL_STEPS[0];
    m_scroll_row_step = 1;
    m_scroll_offset = 0;
    m_scroll_vshift = 0;

    m_vscroll_top = 0;
    m_vscroll_rows = SCREEN_HEIGHT;

    m_scroll_ev.notify(sc_core::SC_ZERO_TIME);

    m_display.render();
}

i2c_response ssd1306::i2c_start(const i2c_target_socket&, tlm::tlm_command) {
    m_await_control = true;
    m_stream_data = false;
    return I2C_ACK;
}

i2c_response ssd1306::i2c_stop(const i2c_target_socket&) {
    m_display.render();
    return I2C_ACK;
}

i2c_response ssd1306::i2c_read(const i2c_target_socket&, u8& data) {
    data = m_display_on ? 0x00 : 0x40;
    return I2C_ACK;
}

i2c_response ssd1306::i2c_write(const i2c_target_socket&, u8 data) {
    if (m_await_control) {
        handle_control_byte(data);
        return I2C_ACK;
    }

    if (m_data_mode)
        write_gddram(data);
    else
        handle_command_byte(data);

    if (!m_stream_data)
        m_await_control = true;

    return I2C_ACK;
}

void ssd1306::handle_control_byte(u8 byte) {
    bool continuation = byte & 0x80;
    m_data_mode = byte & 0x40;

    m_await_control = false;
    m_stream_data = !continuation;
}

void ssd1306::handle_command_byte(u8 byte) {
    if (m_cmd_args_needed > 0) {
        m_cmd_args[m_cmd_args_have++] = byte;
        if (m_cmd_args_have == m_cmd_args_needed) {
            execute_command(m_cmd_opcode, m_cmd_args);
            m_cmd_args_needed = 0;
            m_cmd_args_have = 0;
        }
        return;
    }

    m_cmd_opcode = byte;
    m_cmd_args_have = 0;

    switch (byte) {
    case 0x20: // set memory addressing mode
    case 0x81: // set contrast control
    case 0x8d: // charge pump (not in this datasheet revision, but ubiquitous)
    case 0xa8: // set multiplex ratio
    case 0xd3: // set display offset
    case 0xd5: // set display clock divide ratio / oscillator frequency
    case 0xd9: // set pre-charge period
    case 0xda: // set COM pins hardware configuration
    case 0xdb: // set VCOMH deselect level
        m_cmd_args_needed = 1;
        return;
    case 0x21: // set column address
    case 0x22: // set page address
    case 0xa3: // set vertical scroll area
        m_cmd_args_needed = 2;
        return;
    case 0x29: // continuous vertical and horizontal scroll setup
    case 0x2a:
        m_cmd_args_needed = 5;
        return;
    case 0x26: // horizontal scroll setup
    case 0x27:
        m_cmd_args_needed = 6;
        return;
    default:
        execute_command(byte, nullptr);
        return;
    }
}

void ssd1306::execute_command(u8 opcode, const u8* args) {
    if (opcode <= 0x0f) {
        m_col = (m_col & 0xf0) | (opcode & 0x0f);
        m_page_mode_col_start = m_col;
        return;
    }

    if (0x10 <= opcode && opcode <= 0x1f) {
        m_col = (m_col & 0x0f) | ((opcode & 0x0f) << 4);
        m_page_mode_col_start = m_col;
        return;
    }

    if (0x40 <= opcode && opcode <= 0x7f)
        return;

    if (0xb0 <= opcode && opcode <= 0xb7) {
        m_page = mwr::extract(opcode, 0, 3);
        return;
    }

    switch (opcode) {
    case 0x20:
        switch (args[0] & 0x3) {
        case 0x0:
            m_addr_mode = MODE_HORIZONTAL;
            break;
        case 0x1:
            m_addr_mode = MODE_VERTICAL;
            break;
        case 0x2:
            m_addr_mode = MODE_PAGE;
            break;
        default:
            log_warn("invalid memory addressing mode 0x%x", args[0] & 0x3);
            break;
        }
        break;
    case 0x21:
        m_col_start = mwr::extract(args[0], 0, 7);
        m_col_end = mwr::extract(args[1], 0, 7);
        m_col = m_col_start;
        break;
    case 0x22:
        m_page_start = mwr::extract(args[0], 0, 7);
        m_page_end = mwr::extract(args[1], 0, 7);
        m_page = m_page_start;
        break;
    case 0x26:
    case 0x27:
        m_scroll_left = (opcode == 0x27);
        m_scroll_vertical = false;
        m_scroll_page_start = mwr::extract(args[1], 0, 3);
        m_scroll_frames = SCROLL_STEPS[mwr::extract(args[2], 0, 3)];
        m_scroll_page_end = mwr::extract(args[3], 0, 3);
        break;
    case 0x29:
    case 0x2a:
        m_scroll_left = (opcode == 0x2a);
        m_scroll_vertical = true;
        m_scroll_page_start = mwr::extract(args[1], 0, 3);
        m_scroll_frames = SCROLL_STEPS[mwr::extract(args[2], 0, 3)];
        m_scroll_page_end = mwr::extract(args[3], 0, 3);
        m_scroll_row_step = mwr::extract(args[4], 0, 6);
        break;
    case 0x2e:
        m_scroll_on = false;
        m_scroll_ev.notify(sc_core::SC_ZERO_TIME);
        redraw();
        break;
    case 0x2f:
        m_scroll_on = true;
        m_scroll_offset = 0;
        m_scroll_vshift = 0;
        m_scroll_ev.notify(sc_core::SC_ZERO_TIME);
        break;
    case 0x81:
        m_contrast = args[0];
        break;
    case 0xa0:
        m_segment_remap = false;
        break;
    case 0xa1:
        m_segment_remap = true;
        break;
    case 0xa3:
        m_vscroll_top = mwr::extract(args[0], 0, 6);
        m_vscroll_rows = mwr::extract(args[1], 0, 7);
        break;
    case 0xa4:
        m_entire_display_on = false;
        redraw();
        break;
    case 0xa5:
        m_entire_display_on = true;
        redraw();
        break;
    case 0xa6:
        m_inverse = false;
        redraw();
        break;
    case 0xa7:
        m_inverse = true;
        redraw();
        break;
    case 0xa8:
        m_mux_ratio = mwr::extract(args[0], 0, 6) + 1;
        break;
    case 0xae:
        log_debug("turned display off");
        m_display_on = false;
        redraw();
        break;
    case 0xaf:
        log_debug("turned display on");
        m_display_on = true;
        redraw();
        break;
    case 0xc0:
        m_com_scan_remap = false;
        break;
    case 0xc8:
        m_com_scan_remap = true;
        break;
    case 0xd3:
        m_vertical_shift = mwr::extract(args[0], 0, 6);
        redraw();
        break;
    case 0xd5:
        m_divide_ratio = mwr::extract(args[0], 0, 4);
        m_osc_freq = mwr::extract(args[0], 4, 4);
        break;
    case 0xd9:
        m_phase1_period = mwr::extract(args[0], 0, 4);
        m_phase2_period = mwr::extract(args[0], 4, 4);
        break;
    case 0xdb:
        m_deselect_level = mwr::extract(args[0], 4, 3);
        break;
    case 0xe3:
        break;
    default:
        log_warn("executing unknown command with opcode 0x%02x", opcode);
        break;
    }
}

void ssd1306::write_gddram(u8 byte) {
    m_gddram[m_page][m_col] = byte;

    if (m_scroll_on)
        redraw_all_columns();
    else
        redraw_column(m_page, m_col);

    switch (m_addr_mode) {
    case MODE_PAGE:
        if (++m_col > SCREEN_WIDTH - 1)
            m_col = m_page_mode_col_start;
        break;

    case MODE_HORIZONTAL:
        if (++m_col > m_col_end) {
            m_col = m_col_start;
            if (++m_page > m_page_end)
                m_page = m_page_start;
        }
        break;

    case MODE_VERTICAL:
        if (++m_page > m_page_end) {
            m_page = m_page_start;
            if (++m_col > m_col_end)
                m_col = m_col_start;
        }
        break;
    }
}

void ssd1306::set_pixel(u32 row, u32 col, bool on) {
    u32 color = on ? color_on : color_off;
    u8* p = &m_video[(row * SCREEN_WIDTH + col) * 3];
    p[0] = mwr::extract(color, 16, 8);
    p[1] = mwr::extract(color, 8, 8);
    p[2] = mwr::extract(color, 0, 8);
}

void ssd1306::fill_all_pixels(u32 color) {
    u8 r = mwr::extract(color, 16, 8);
    u8 g = mwr::extract(color, 8, 8);
    u8 b = mwr::extract(color, 0, 8);

    for (size_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        m_video[i * 3 + 0] = r;
        m_video[i * 3 + 1] = g;
        m_video[i * 3 + 2] = b;
    }
}

bool ssd1306::gddram_bit(u32 row, u32 col) const {
    return (m_gddram[row / 8][col] >> (row % 8)) & 1;
}

void ssd1306::scroll_source(u32 row, u32 col, u32& src_row,
                            u32& src_col) const {
    src_row = row;
    src_col = col;

    if (!m_scroll_on)
        return;

    uint page = row / 8;
    if (page >= m_scroll_page_start && page <= m_scroll_page_end) {
        u32 shift = m_scroll_offset % SCREEN_WIDTH;
        src_col = m_scroll_left ? (col + shift) % SCREEN_WIDTH
                                : (col + SCREEN_WIDTH - shift) % SCREEN_WIDTH;
    }

    if (m_scroll_vertical && m_vscroll_rows > 0) {
        u32 area_start = m_vscroll_top;
        u32 area_end = std::min<u32>(area_start + m_vscroll_rows,
                                     SCREEN_HEIGHT);
        if (row >= area_start && row < area_end) {
            u32 area_rows = area_end - area_start;
            u32 rel = row - area_start;
            u32 src_rel = (rel + m_scroll_vshift) % area_rows;
            src_row = area_start + src_rel;
        }
    }
}

void ssd1306::redraw_column(uint page, uint col) {
    u32 out_col = (m_segment_remap ^ rotated) ? (SCREEN_WIDTH - 1 - col) : col;

    for (uint bit = 0; bit < 8; bit++) {
        u32 row = page * 8 + bit;

        u32 src_row, src_col;
        scroll_source(row, col, src_row, src_col);
        bool set = gddram_bit(src_row, src_col);

        bool on_logical = m_entire_display_on || (set != m_inverse);
        bool on = m_display_on && on_logical;

        u32 out_row = (m_com_scan_remap ^ rotated) ? (SCREEN_HEIGHT - 1 - row)
                                                   : row;
        out_row = (out_row + m_vertical_shift) % SCREEN_HEIGHT;
        set_pixel(out_row, out_col, on);
    }
}

void ssd1306::redraw_all_columns() {
    for (uint page = 0; page < NUM_PAGES; page++)
        for (uint col = 0; col < SCREEN_WIDTH; col++)
            redraw_column(page, col);
}

void ssd1306::redraw() {
    redraw_all_columns();
    m_display.render();
}

void ssd1306::advance_scroll() {
    m_scroll_offset = (m_scroll_offset + 1) % SCREEN_WIDTH;

    if (m_scroll_vertical && m_vscroll_rows > 0) {
        u32 area_rows = std::min<u32>(m_vscroll_rows,
                                      SCREEN_HEIGHT - m_vscroll_top);
        if (area_rows > 0)
            m_scroll_vshift = (m_scroll_vshift + m_scroll_row_step) %
                              area_rows;
    }
}

void ssd1306::scroll_thread() {
    while (true) {
        if (!m_scroll_on) {
            wait(m_scroll_ev);
            continue;
        }

        sc_time deadline = sc_core::sc_time_stamp() + scroll_step_period();

        wait(scroll_step_period(), m_scroll_ev);

        if (sc_core::sc_time_stamp() < deadline)
            continue;

        advance_scroll();
        redraw();
    }
}

VCML_EXPORT_MODEL(vcml::i2c::ssd1306, name, args) {
    return new ssd1306(name);
}

} // namespace i2c
} // namespace vcml
