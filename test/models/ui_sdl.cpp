/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

#include <chrono>
#include <condition_variable>

class sdl_monitor : public mwr::publisher
{
private:
    mutable mutex m_mtx;
    std::condition_variable m_cv;
    string m_driver;
    bool m_started;
    bool m_failed;

public:
    sdl_monitor():
        mwr::publisher(LOG_ERROR, LOG_DEBUG),
        m_mtx(),
        m_cv(),
        m_driver(),
        m_started(false),
        m_failed(false) {}

    virtual ~sdl_monitor() = default;

    virtual void publish(const mwr::logmsg& msg) override {
        static const string started = "SDL: using videodriver \"";
        static const string failed = "cannot initialize SDL";

        for (const string& line : msg.lines) {
            size_t pos = line.find(started);
            if (pos != string::npos) {
                lock_guard<mutex> guard(m_mtx);
                m_driver = line.substr(pos + started.length());
                m_driver = m_driver.substr(0, m_driver.find('"'));
                m_started = true;
                m_cv.notify_all();
            }

            if (line.find(failed) != string::npos) {
                lock_guard<mutex> guard(m_mtx);
                m_failed = true;
                m_cv.notify_all();
            }
        }
    }

    bool wait_started(string& driver) {
        std::unique_lock<mutex> guard(m_mtx);
        bool done = m_cv.wait_for(guard, std::chrono::seconds(30),
                                  [&]() { return m_started || m_failed; });

        EXPECT_TRUE(done) << "timeout waiting for SDL ui thread";
        driver = m_driver;
        return done && m_started;
    }
};

class ui_sdl_test : public test_base
{
public:
    using mode_fn = ui::videomode (*)(u32, u32);

    sdl_monitor monitor;

    ui::display disp;
    ui::console con;

    ui::keyboard kbd;
    ui::keyboard kbd_us;
    ui::mouse ptr;
    ui::touchpad pad;
    ui::multitouch touch;

    bool have_sdl;

    ui_sdl_test(const sc_module_name& nm):
        test_base(nm),
        monitor(),
        disp("display"),
        con("console"),
        kbd("keyboard"),
        kbd_us("keyboard_us", "us"),
        ptr("mouse"),
        pad("touchpad"),
        touch("multitouch"),
        have_sdl(false) {
        con.display = disp.name();

#ifndef MWR_MACOS
        try {
            ui::backend* b = ui::backend::create("sdl:0");
            have_sdl = !strcmp(b->type(), "sdl");
            ui::backend::destroy(b);
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
        }
#endif

        add_test("strings", &ui_sdl_test::test_strings);
        add_test("create", &ui_sdl_test::test_create);
        add_test("init", &ui_sdl_test::test_init);
        add_test("videomodes", &ui_sdl_test::test_videomodes);
        add_test("render", &ui_sdl_test::test_render);
        add_test("reinit", &ui_sdl_test::test_reinit);
        add_test("multiple", &ui_sdl_test::test_multiple);
        add_test("console", &ui_sdl_test::test_console);
        add_test("input", &ui_sdl_test::test_input);
    }

    bool skip(const char* test) {
        if (!have_sdl)
            log_warn("sdl display unavailable, skipping %s", test);
        return !have_sdl;
    }

    void let_ui_thread_run() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    static vector<mode_fn> all_modes() {
        return {
            ui::videomode::a8r8g8b8, ui::videomode::x8r8g8b8,
            ui::videomode::r8g8b8a8, ui::videomode::r8g8b8x8,
            ui::videomode::a8b8g8r8, ui::videomode::x8b8g8r8,
            ui::videomode::b8g8r8a8, ui::videomode::b8g8r8x8,
            ui::videomode::r8g8b8,   ui::videomode::b8g8r8,
            ui::videomode::r5g6b5,   ui::videomode::b5g6r5,
        };
    }

    static vector<u8> make_framebuffer(const ui::videomode& mode, u8 seed) {
        vector<u8> fb(mode.size);
        for (size_t i = 0; i < fb.size(); i++)
            fb[i] = (u8)(seed + i * 7);
        return fb;
    }

    void expect_event(ui::input& dev, u16 type, u16 code, i32 state) {
        ui::input_event ev{};
        ASSERT_TRUE(dev.pop_event(ev)) << "no event from " << dev.name();
        EXPECT_EQ(ev.type, type) << dev.name();
        EXPECT_EQ(ev.code, code) << dev.name();
        EXPECT_EQ(ev.state, state) << dev.name();
    }

    void drain(ui::input& dev) {
        ui::input_event ev{};
        while (dev.pop_event(ev))
            continue;
    }

    void test_strings() {
        EXPECT_STREQ(con.kind(), "vcml::ui::console");
        EXPECT_STREQ(con.version(), VCML_VERSION_STRING);
    }

    void test_create() {
        if (skip("create"))
            return;

        EXPECT_THROW(ui::backend::create("nothing:0"), std::exception);
        EXPECT_THROW(ui::backend::create("sdl"), std::exception);
        EXPECT_THROW(ui::backend::create("sdl:"), std::exception);
        EXPECT_THROW(ui::backend::create("sdl:x"), std::exception);

        ui::backend* b = ui::backend::create("sdl:3");
        ASSERT_NE(b, nullptr);
        EXPECT_STREQ(b->type(), "sdl");
        EXPECT_STREQ(b->name(), "sdl:3");
        EXPECT_EQ(b->id(), 3);
        EXPECT_FALSE(b->has_framebuffer());
        EXPECT_EQ(b->framebuffer(), nullptr);
        EXPECT_EQ(b->framebuffer_size(), 0);
        EXPECT_THROW(b->handle_option("fullscreen"), std::exception);

        stringstream ss;
        b->to_json(ss);
        EXPECT_EQ(ss.str(), "{\"type\":\"sdl\",\"name\":\"sdl:3\",\"id\":3}");

        b->shutdown();
        EXPECT_FALSE(b->has_framebuffer());
        ui::backend::destroy(b);
    }

    void test_init() {
        if (skip("init"))
            return;

        auto mode = ui::videomode::a8r8g8b8(320, 200);
        vector<u8> fb = make_framebuffer(mode, 1);

        ui::backend* b = ui::backend::create("sdl:0");
        b->init(mode, fb.data());
        EXPECT_TRUE(b->has_framebuffer());
        EXPECT_EQ(b->framebuffer(), fb.data());
        EXPECT_EQ(b->framebuffer_size(), mode.size);
        EXPECT_EQ(b->mode(), mode);
        EXPECT_EQ(b->xres(), 320);
        EXPECT_EQ(b->yres(), 200);

        string driver;
        if (monitor.wait_started(driver)) {
            EXPECT_EQ(driver, "dummy");
            let_ui_thread_run();
        } else {
            log_warn("sdl failed to initialize, skipping remaining tests");
            have_sdl = false;
        }

        b->shutdown();
        EXPECT_FALSE(b->has_framebuffer());
        EXPECT_EQ(b->framebuffer(), nullptr);
        ui::backend::destroy(b);
    }

    void test_videomodes() {
        if (skip("videomodes"))
            return;

        u32 n = 0;
        for (mode_fn fn : all_modes()) {
            auto mode = fn(64 + 32 * n, 48 + 16 * n);
            vector<u8> fb = make_framebuffer(mode, (u8)n);

            ui::backend* b = ui::backend::create(mkstr("sdl:%u", n));
            b->setup(mode, fb.data());
            EXPECT_EQ(b->mode(), mode) << mode;
            EXPECT_EQ(b->framebuffer(), fb.data()) << mode;
            EXPECT_EQ(b->framebuffer_size(), mode.size) << mode;
            let_ui_thread_run();
            ui::backend::destroy(b);
            n++;
        }

        ui::backend* b = ui::backend::create("sdl:0");
        for (mode_fn fn : all_modes()) {
            auto mode = fn(64 + 32 * n, 48 + 16 * n);
            b->setup(mode, nullptr);
            EXPECT_EQ(b->mode(), mode) << mode;
            ASSERT_NE(b->framebuffer(), nullptr) << mode;
            EXPECT_EQ(b->framebuffer_size(), mode.size) << mode;
            let_ui_thread_run();
            n--;
        }

        ui::backend::destroy(b);
    }

    void test_render() {
        if (skip("render"))
            return;

        auto mode = ui::videomode::r5g6b5(200, 100);
        vector<u8> fb = make_framebuffer(mode, 0x11);

        ui::backend* b = ui::backend::create("sdl:1");
        b->render(0, 0, 1, 1);
        b->setup(mode, fb.data());
        b->render(0, 0, mode.xres, mode.yres);
        let_ui_thread_run();
        b->render(0, 0, mode.xres, mode.yres);
        b->render(10, 10, 20, 20);
        let_ui_thread_run();

        b->shutdown();
        fb = make_framebuffer(mode, 0x99);
        b->setup(mode, fb.data());
        EXPECT_EQ(b->framebuffer(), fb.data());
        b->render(0, 0, mode.xres, mode.yres);
        let_ui_thread_run();

        ui::backend::destroy(b);
    }

    void test_reinit() {
        if (skip("reinit"))
            return;

        vector<mode_fn> modes = all_modes();
        ui::backend* b = ui::backend::create("sdl:2");
        b->setup(ui::videomode::a8r8g8b8(64, 64), nullptr);
        let_ui_thread_run();

        for (u32 i = 0; i < 2000; i++) {
            u32 w = 16 + 8 * (i % 13);
            u32 h = 16 + 8 * (i % 7);
            auto mode = modes[i % modes.size()](w, h);
            b->reinit(mode, nullptr);
            ASSERT_EQ(b->mode(), mode);
            ASSERT_NE(b->framebuffer(), nullptr);
            if (i % 100 == 0)
                std::this_thread::yield();
        }

        let_ui_thread_run();
        b->shutdown();
        b->shutdown();
        EXPECT_FALSE(b->has_framebuffer());

        b->setup(ui::videomode::b8g8r8(48, 32), nullptr);
        EXPECT_TRUE(b->has_framebuffer());
        let_ui_thread_run();
        ui::backend::destroy(b);
    }

    void test_multiple() {
        if (skip("multiple"))
            return;

        auto mode0 = ui::videomode::a8r8g8b8(160, 120);
        auto mode1 = ui::videomode::b8g8r8(96, 64);
        vector<u8> fb0 = make_framebuffer(mode0, 3);
        vector<u8> fb1 = make_framebuffer(mode1, 5);

        ui::backend* b0 = ui::backend::create("sdl:0");
        ui::backend* b1 = ui::backend::create("sdl:1");
        ui::backend* b2 = ui::backend::create("sdl:1");
        ASSERT_NE(b0, b1);
        ASSERT_NE(b1, b2);

        b0->setup(mode0, fb0.data());
        b1->setup(mode1, fb1.data());
        b2->setup(mode1, fb1.data());
        let_ui_thread_run();

        EXPECT_EQ(b0->mode(), mode0);
        EXPECT_EQ(b1->mode(), mode1);
        EXPECT_EQ(b2->mode(), mode1);

        ui::backend::destroy(b1);
        let_ui_thread_run();
        EXPECT_EQ(b0->framebuffer(), fb0.data());
        EXPECT_EQ(b2->framebuffer(), fb1.data());

        b0->setup(mode1, fb1.data());
        let_ui_thread_run();
        EXPECT_EQ(b0->mode(), mode1);

        ui::backend::destroy(b2);
        ui::backend::destroy(b0);
        let_ui_thread_run();

        b0 = ui::backend::create("sdl:0");
        b0->setup(mode0, fb0.data());
        let_ui_thread_run();
        ui::backend::destroy(b0);

        b0 = ui::backend::create("sdl:0");
        b0->setup(mode0, fb0.data());
        ui::backend::destroy(b0);
    }

    void test_console() {
        if (skip("console"))
            return;

        auto mode0 = ui::videomode::x8r8g8b8(160, 120);
        auto mode1 = ui::videomode::r8g8b8(80, 60);
        vector<u8> fb0 = make_framebuffer(mode0, 7);
        vector<u8> fb1 = make_framebuffer(mode1, 9);

        stringstream ss;
        EXPECT_FALSE(con.execute("attach_backend", { "nothing:0" }, ss));
        EXPECT_THAT(ss.str(), HasSubstr("error creating backend"));

        disp.setup(mode0, fb0.data());
        EXPECT_EQ(disp.xres(), mode0.xres);
        EXPECT_EQ(disp.yres(), mode0.yres);

        ss.str("");
        ASSERT_TRUE(con.execute("attach_backend", { "sdl:7" }, ss));
        EXPECT_EQ(ss.str(), "created backend 0");

        ss.str("");
        EXPECT_TRUE(con.execute("list_backends", ss));
        EXPECT_EQ(ss.str(), "0: sdl,");

        ss.str("");
        EXPECT_TRUE(con.execute("list_backends", { "0", "1" }, ss));
        EXPECT_EQ(ss.str(), "0: sdl,1: none,");

        ss.str("");
        EXPECT_TRUE(con.execute("list_backends_json", ss));
        EXPECT_EQ(ss.str(),
                  "{\"0\":{\"type\":\"sdl\",\"name\":\"sdl:7\",\"id\":7}}");

        disp.render();
        disp.render(1, 2, 3, 4);
        let_ui_thread_run();

        disp.setup(mode1, fb1.data());
        disp.render();
        let_ui_thread_run();

        ss.str("");
        EXPECT_TRUE(con.execute("detach_backend", { "0" }, ss));
        EXPECT_EQ(ss.str(), "");
        EXPECT_TRUE(con.execute("detach_backend", { "0" }, ss));
        EXPECT_EQ(ss.str(), "invalid backend id: 0");

        ss.str("");
        EXPECT_TRUE(con.execute("list_backends_json", ss));
        EXPECT_EQ(ss.str(), "{}");

        disp.setup(ui::videomode(), nullptr);
    }

    void test_input() {
        if (skip("input"))
            return;

        auto mode = ui::videomode::a8r8g8b8(320, 200);
        vector<u8> fb = make_framebuffer(mode, 0x42);

        ui::backend* b = ui::backend::create("sdl:4");
        b->setup(mode, fb.data());
        b->attach(&kbd);
        b->attach(&kbd_us);
        b->attach(&ptr);
        b->attach(&pad);
        b->attach(&touch);
        EXPECT_EQ(b->framebuffer(), fb.data());
        let_ui_thread_run();

        b->notify_key(ui::KEYSYM_a, true);
        b->notify_key(ui::KEYSYM_a, true);
        b->notify_key(ui::KEYSYM_a, false);
        expect_event(kbd, ui::EV_KEY, ui::KEYSYM_a, 1);
        expect_event(kbd, ui::EV_SYN, ui::SYN_REPORT, 0);
        drain(kbd);
        expect_event(kbd_us, ui::EV_KEY, ui::KEY_A, 1);
        expect_event(kbd_us, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(kbd_us, ui::EV_KEY, ui::KEY_A, 2);
        expect_event(kbd_us, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(kbd_us, ui::EV_KEY, ui::KEY_A, 0);
        expect_event(kbd_us, ui::EV_SYN, ui::SYN_REPORT, 0);
        EXPECT_FALSE(kbd_us.has_events());

        b->notify_key(ui::KEYSYM_LEFTSHIFT, true);
        EXPECT_TRUE(kbd_us.shift_l());
        EXPECT_TRUE(kbd_us.shift());
        b->notify_key(ui::KEYSYM_LEFTSHIFT, false);
        EXPECT_FALSE(kbd_us.shift());
        drain(kbd);
        drain(kbd_us);
        EXPECT_FALSE(ptr.has_events());
        EXPECT_FALSE(pad.has_events());
        EXPECT_FALSE(touch.has_events());

        b->notify_pos(319, 199);
        EXPECT_EQ(ptr.x(), 10000);
        EXPECT_EQ(ptr.y(), 10000);
        expect_event(ptr, ui::EV_REL, ui::REL_X, 10000);
        expect_event(ptr, ui::EV_REL, ui::REL_Y, 10000);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        EXPECT_FALSE(ptr.has_events());

        b->notify_pos(5000, 5000);
        EXPECT_EQ(ptr.x(), 10000);
        EXPECT_EQ(ptr.y(), 10000);
        EXPECT_FALSE(ptr.has_events());

        b->notify_pos(0, 0);
        EXPECT_EQ(ptr.x(), 0);
        EXPECT_EQ(ptr.y(), 0);
        expect_event(ptr, ui::EV_REL, ui::REL_X, -10000);
        expect_event(ptr, ui::EV_REL, ui::REL_Y, -10000);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        EXPECT_EQ(pad.x(), 0);
        EXPECT_EQ(pad.y(), 0);
        EXPECT_FALSE(pad.has_events());
        EXPECT_FALSE(touch.has_events());
        EXPECT_FALSE(kbd.has_events());

        b->notify_btn(ui::BUTTON_LEFT, true);
        EXPECT_TRUE(ptr.left());
        EXPECT_TRUE(pad.is_touching());
        EXPECT_TRUE(touch.is_touching());
        expect_event(ptr, ui::EV_KEY, ui::BTN_LEFT, 1);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(pad, ui::EV_KEY, ui::BTN_TOUCH, 1);
        expect_event(pad, ui::EV_ABS, ui::ABS_X, 0);
        expect_event(pad, ui::EV_ABS, ui::ABS_Y, 0);
        expect_event(pad, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_SLOT, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_TRACKING_ID, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_POSITION_X, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_POSITION_Y, 0);
        expect_event(touch, ui::EV_SYN, ui::SYN_REPORT, 0);

        b->notify_pos(319, 0);
        expect_event(ptr, ui::EV_REL, ui::REL_X, 10000);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(pad, ui::EV_ABS, ui::ABS_X, 10000);
        expect_event(pad, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_SLOT, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_POSITION_X, 10000);
        expect_event(touch, ui::EV_SYN, ui::SYN_REPORT, 0);

        b->notify_btn(ui::BUTTON_LEFT, false);
        EXPECT_FALSE(ptr.left());
        EXPECT_FALSE(pad.is_touching());
        EXPECT_FALSE(touch.is_touching());
        expect_event(ptr, ui::EV_KEY, ui::BTN_LEFT, 0);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(pad, ui::EV_KEY, ui::BTN_TOUCH, 0);
        expect_event(pad, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_SLOT, 0);
        expect_event(touch, ui::EV_ABS, ui::ABS_MT_TRACKING_ID, -1);
        expect_event(touch, ui::EV_SYN, ui::SYN_REPORT, 0);

        b->notify_btn(ui::BUTTON_RIGHT, true);
        EXPECT_TRUE(ptr.right());
        b->notify_btn(ui::BUTTON_RIGHT, false);
        EXPECT_FALSE(ptr.right());
        expect_event(ptr, ui::EV_KEY, ui::BTN_RIGHT, 1);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(ptr, ui::EV_KEY, ui::BTN_RIGHT, 0);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);

        b->notify_btn(ui::BUTTON_WHEEL_UP, true);
        b->notify_btn(ui::BUTTON_WHEEL_UP, false);
        b->notify_btn(ui::BUTTON_WHEEL_DOWN, true);
        b->notify_btn(ui::BUTTON_WHEEL_DOWN, false);
        expect_event(ptr, ui::EV_REL, ui::REL_WHEEL, 1);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(ptr, ui::EV_REL, ui::REL_WHEEL, 0);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(ptr, ui::EV_REL, ui::REL_WHEEL, -1);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        expect_event(ptr, ui::EV_REL, ui::REL_WHEEL, 0);
        expect_event(ptr, ui::EV_SYN, ui::SYN_REPORT, 0);
        EXPECT_FALSE(ptr.has_events());
        EXPECT_FALSE(kbd.has_events());

        drain(pad);
        drain(touch);

        b->detach(&ptr);
        b->notify_btn(ui::BUTTON_MIDDLE, true);
        b->notify_btn(ui::BUTTON_MIDDLE, false);
        EXPECT_FALSE(ptr.has_events());
        EXPECT_TRUE(pad.has_events());
        EXPECT_TRUE(touch.has_events());

        drain(pad);
        drain(touch);

        b->detach(&kbd);
        b->detach(&kbd_us);
        b->detach(&pad);
        b->detach(&touch);
        b->notify_key(ui::KEYSYM_a, true);
        b->notify_key(ui::KEYSYM_a, false);
        EXPECT_FALSE(kbd.has_events());
        EXPECT_FALSE(kbd_us.has_events());

        ui::backend::destroy(b);
    }
};

TEST(ui, sdl) {
    mwr::setenv("SDL_VIDEODRIVER", "dummy");
    mwr::setenv("SDL_AUDIODRIVER", "dummy");
    ui_sdl_test test("test");
    sc_core::sc_start();
}
