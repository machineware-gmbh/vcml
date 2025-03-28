/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_VNC_H
#define VCML_UI_VNC_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/logger.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/video.h"
#include "vcml/ui/display.h"

namespace vcml {
namespace ui {

enum vnc_requests : u8 {
    VNC_SET_PIXEL_FORMAT = 0,
    VNC_SET_ENCODINGS = 2,
    VNC_FRAMEBUFFER_REQUEST = 3,
    VNC_KEY_EVENT = 4,
    VNC_POINTER_EVENT = 5,
    VNC_CLIENT_CUT_TEXT = 6,
};

enum vnc_responses : u8 {
    VNC_FRAMEBUFFER_UPDATE = 0,
    VNC_SET_COLOR_MAP = 1,
    VNC_BELL = 2,
    VNC_SERVER_CUT_TEXT = 3,
};

enum vnc_buttons : u32 {
    VNC_BTN_LEFT = 1 << 0,
    VNC_BTN_MIDDLE = 1 << 1,
    VNC_BTN_RIGHT = 1 << 2,
    VNC_BTN_WHEEL_UP = 1 << 3,
    VNC_BTN_WHEEL_DOWN = 1 << 4,
    VNC_BTN_WHEEL_LEFT = 1 << 5,
    VNC_BTN_WHEEL_RIGHT = 1 << 6,
};

enum vnc_encodings : i32 {
    VNC_ENC_RAW = 0,
    VNC_ENC_COPYRECT = 1,
    VNC_ENC_RRE = 2,
    VNC_ENC_HEXTILE = 5,
    VNC_ENC_TRLE = 15,
    VNC_ENC_ZRLE = 16,
    VNC_ENC_CURSOR = -239,
    VNC_ENC_DESKTOP_SIZE = -223,
};

struct vnc_pixelformat {
    u8 bpp;
    u8 depth;
    u8 endian;
    u8 truecolor;
    u16 rmax;
    u16 gmax;
    u16 bmax;
    u8 roff;
    u8 goff;
    u8 boff;
};

class vnc : public display
{
private:
    u16 m_port;
    u8 m_buttons;
    u16 m_ptr_x;
    u16 m_ptr_y;
    i32 m_encoding;
    vnc_pixelformat m_format;

    mwr::socket m_socket;

    atomic<bool> m_running;
    mutex m_mutex;
    thread m_thread;

    template <typename T>
    T vnc_swap(const T& val) {
        return host_endian() == ENDIAN_BIG ? val : bswap(val);
    }

    template <typename T>
    void send(const T& val) {
        m_socket.send(vnc_swap(val));
    }

    template <typename T>
    T recv() {
        T val;
        m_socket.recv(val);
        return vnc_swap(val);
    }

    void send_padding(size_t n) {
        while (n--)
            send<u8>(0);
    }

    void recv_padding(size_t n) {
        while (n--)
            recv<u8>();
    }

    void send_framebuffer_raw();
    void send_framebuffer_hextile(u32 x, u32 y, u32 w, u32 h,
                                  optional<u32>& bg, optional<u32>& fg);
    void send_framebuffer_hextile();

    void handshake();
    void handle_set_encodings(const vector<i32>& encodings);
    void handle_set_pixel_format(const vnc_pixelformat& fmt);
    void handle_framebuffer_request(u8 inc, u16 x, u16 y, u16 w, u16 h);
    void handle_key_event(u32 key, u8 down);
    void handle_ptr_event(u8 buttons, u16 x, u16 y);
    void handle_cut_text(const string& text);
    void handle_command();
    void run();

public:
    u16 port() const { return m_port; }

    vnc(u32 nr);
    virtual ~vnc();

    virtual void init(const videomode& mode, u8* fb) override;
    virtual void shutdown() override;

    static display* create(u32 nr);
};

} // namespace ui
} // namespace vcml

#endif
