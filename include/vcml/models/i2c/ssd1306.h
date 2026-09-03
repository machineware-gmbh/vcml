/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_I2C_SSD1306_H
#define VCML_I2C_SSD1306_H

#include "vcml/core/model.h"
#include "vcml/core/component.h"
#include "vcml/core/systemc.h"
#include "vcml/core/types.h"

#include "vcml/protocols/i2c.h"

#include "vcml/ui/display.h"

namespace vcml {
namespace i2c {

class ssd1306 : public component, public i2c_host
{
public:
    property<bool> alternate_address;
    property<bool> rotated;
    property<u32> color_off;
    property<u32> color_on;

    i2c_target_socket i2c_in;

    ssd1306(const sc_core::sc_module_name& nm);
    virtual ~ssd1306() = default;
    VCML_KIND(i2c::ssd1306);
    virtual void reset() override;

    bool is_display_on() const;
    bool is_inverse() const;
    u8 get_contrast() const;
    bool read_pixel(u32 x, u32 y) const;

    bool is_scrolling() const;
    sc_core::sc_time frame_period() const;

protected:
    static constexpr u32 SCREEN_WIDTH = 128;
    static constexpr u32 SCREEN_HEIGHT = 64;
    static constexpr u32 NUM_PAGES = SCREEN_HEIGHT / 8;
    static constexpr ui::pixelformat PIXEL_FORMAT = ui::FORMAT_R8G8B8;

    enum addressing_mode { MODE_PAGE, MODE_HORIZONTAL, MODE_VERTICAL };

    virtual i2c_response i2c_start(const i2c_target_socket&,
                                   tlm::tlm_command) override;
    virtual i2c_response i2c_stop(const i2c_target_socket&) override;
    virtual i2c_response i2c_read(const i2c_target_socket&, u8& data) override;
    virtual i2c_response i2c_write(const i2c_target_socket&, u8 data) override;

    virtual void end_of_elaboration() override;

    void handle_control_byte(u8 byte);
    void handle_command_byte(u8 byte);
    void execute_command(u8 opcode, const u8* args);
    void write_gddram(u8 byte);

    void redraw_column(uint page, uint col);
    void redraw_all_columns();
    void redraw();

    void set_pixel(u32 row, u32 col, bool on);
    void fill_all_pixels(u32 color);

    bool gddram_bit(u32 row, u32 col) const;
    void scroll_source(u32 row, u32 col, u32& src_row, u32& src_col) const;
    sc_core::sc_time scroll_step_period() const;
    void advance_scroll();
    void scroll_thread();

    ui::display m_display;

    u8 m_video[SCREEN_WIDTH * SCREEN_HEIGHT * 3];
    u8 m_gddram[NUM_PAGES][SCREEN_WIDTH];

    bool m_await_control;
    bool m_stream_data;
    bool m_data_mode;

    u8 m_cmd_opcode;
    u8 m_cmd_args[8];
    int m_cmd_args_needed;
    int m_cmd_args_have;

    addressing_mode m_addr_mode;
    u8 m_col, m_page;
    u8 m_col_start, m_col_end;
    u8 m_page_start, m_page_end;
    u8 m_page_mode_col_start;
    u8 m_contrast;
    u8 m_divide_ratio;
    u8 m_osc_freq;
    u8 m_phase1_period;
    u8 m_phase2_period;
    u8 m_deselect_level;
    u8 m_vertical_shift;
    u8 m_mux_ratio;

    bool m_display_on;
    bool m_inverse;
    bool m_entire_display_on;
    bool m_segment_remap;
    bool m_com_scan_remap;

    bool m_scroll_on;
    bool m_scroll_left;
    bool m_scroll_vertical;
    u8 m_scroll_page_start, m_scroll_page_end;
    u16 m_scroll_frames;
    u8 m_scroll_row_step;
    u32 m_scroll_offset;
    u32 m_scroll_vshift;

    u8 m_vscroll_top;
    u8 m_vscroll_rows;

    sc_core::sc_event m_scroll_ev;
};

} // namespace i2c
} // namespace vcml

#endif
