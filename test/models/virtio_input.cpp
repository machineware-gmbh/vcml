/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "virtio_test.h"

class virtio_rng_stim : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    virtio::mmio virtio;
    virtio::input virtio_input;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    virtio_stim stim;
    virtio_stim tstim;
    virtio_stim mstim;
    virtio::input input;
    virtio::input touch;
    virtio::input mtouch;

    enum : u32 {
        VQ_EVENT = 0,
        VQ_STATUS = 1,
    };

    enum : u8 {
        CFG_UNSET = 0x00,
        CFG_ID_NAME = 0x01,
        CFG_ID_SERIAL = 0x02,
        CFG_ID_DEVIDS = 0x03,
        CFG_PROP_BITS = 0x10,
        CFG_EV_BITS = 0x11,
        CFG_ABS_INFO = 0x12,
    };

    enum : u64 {
        CFG_SELECT = 0,
        CFG_SUBSEL = 1,
        CFG_SIZE = 2,
        CFG_DATA = 8,
        CFG_END = 136,
    };

    virtio_rng_stim(const sc_module_name& nm = sc_gen_unique_name("stim")):
        test_base(nm),
        bus("bus"),
        mem("mem", 0x1000),
        virtio("virtio"),
        virtio_input("virtio_input"),
        out("out"),
        irq("irq"),
        stim("stim"),
        tstim("tstim"),
        mstim("mstim"),
        input("input"),
        touch("touch"),
        mtouch("mtouch") {
        stim.virtio_out.bind(input.virtio_in);
        tstim.virtio_out.bind(touch.virtio_in);
        mstim.virtio_out.bind(mtouch.virtio_in);

        add_test("mmio", &virtio_rng_stim::test_mmio);
        add_test("identify", &virtio_rng_stim::test_identify);
        add_test("config_ids", &virtio_rng_stim::test_config_ids);
        add_test("config_events", &virtio_rng_stim::test_config_events);
        add_test("config_absinfo", &virtio_rng_stim::test_config_absinfo);
        add_test("keyboard", &virtio_rng_stim::test_keyboard);
        add_test("mouse", &virtio_rng_stim::test_mouse);
        add_test("touchpad", &virtio_rng_stim::test_touchpad);
        add_test("multitouch", &virtio_rng_stim::test_multitouch);
        add_test("backpressure", &virtio_rng_stim::test_backpressure);

        virtio.virtio_out.bind(virtio_input.virtio_in);

        bus.bind(mem.in, 0, 0xfff);
        bus.bind(virtio.in, 0x1000, 0x1fff);

        bus.bind(out);
        bus.bind(virtio.out);

        virtio.irq.bind(irq);

        clk.bind(bus.clk);
        clk.bind(mem.clk);
        clk.bind(virtio.clk);

        rst.bind(bus.rst);
        rst.bind(mem.rst);
        rst.bind(virtio.rst);
    }

    void test_mmio() {
        enum addresses : u64 {
            INPUT_BASE = 0x1000,
            INPUT_MAGIC = INPUT_BASE + 0x00,
            INPUT_VERSION = INPUT_BASE + 0x04,
            INPUT_DEVID = INPUT_BASE + 0x08,
            INPUT_VQ_SEL = INPUT_BASE + 0x30,
            INPUT_VQ_MAX = INPUT_BASE + 0x34,
            INPUT_STATUS = INPUT_BASE + 0x70,
        };

        u32 data;
        ASSERT_OK(out.readw(INPUT_MAGIC, data));
        ASSERT_EQ(data, 0x74726976);

        ASSERT_OK(out.readw(INPUT_VERSION, data));
        ASSERT_EQ(data, 2);

        ASSERT_OK(out.readw(INPUT_DEVID, data));
        ASSERT_EQ(data, VIRTIO_DEVICE_INPUT);

        ASSERT_OK(out.readw(INPUT_STATUS, data));
        ASSERT_EQ(data, 0);

        data = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK;
        ASSERT_OK(out.writew(INPUT_STATUS, data));

        ASSERT_OK(out.readw(INPUT_STATUS, data));
        ASSERT_TRUE(data & VIRTIO_STATUS_FEATURES_OK);

        data = 0;
        ASSERT_OK(out.writew(INPUT_VQ_SEL, data));
        ASSERT_OK(out.readw(INPUT_VQ_MAX, data));
        EXPECT_EQ(data, 8);

        data = 1;
        ASSERT_OK(out.writew(INPUT_VQ_SEL, data));
        ASSERT_OK(out.readw(INPUT_VQ_MAX, data));
        EXPECT_EQ(data, 8);

        data = 2;
        ASSERT_OK(out.writew(INPUT_VQ_SEL, data));
        ASSERT_OK(out.readw(INPUT_VQ_MAX, data));
        EXPECT_EQ(data, 0);
    }

    using event = ui::input_event;
    using events = vector<event>;

    static string describe(const events& evs) {
        stringstream ss;
        for (const event& ev : evs)
            ss << "{" << ev.type << "," << ev.code << "," << ev.state << "} ";
        return ss.str();
    }

    void expect_events(virtio_stim& s, const events& expected) {
        wait(sc_time(5, SC_MS));
        events actual;
        for (const vq_message& msg : s.used[VQ_EVENT]) {
            vector<u8> data = s.response(msg);
            event ev = {};
            EXPECT_EQ(data.size(), sizeof(ev));
            if (data.size() == sizeof(ev))
                memcpy(&ev, data.data(), sizeof(ev));
            actual.push_back(ev);
        }

        EXPECT_EQ(describe(actual), describe(expected));
        s.used[VQ_EVENT].clear();
    }

    void prepare(virtio_stim& s, size_t nbufs = 8) {
        s.clear();
        s.identify();
        for (size_t i = 0; i < nbufs; i++)
            s.submit(VQ_EVENT, {}, { sizeof(event) });
        EXPECT_TRUE(s.kick(VQ_EVENT));
        EXPECT_TRUE(s.avail[VQ_EVENT].empty());
        EXPECT_TRUE(s.used[VQ_EVENT].empty());
    }

    vector<u8> select(virtio_stim& s, u8 sel, u8 subsel) {
        EXPECT_TRUE(s.write_config(CFG_SUBSEL, subsel));
        EXPECT_TRUE(s.write_config(CFG_SELECT, sel));

        u8 size = 0;
        EXPECT_TRUE(s.read_config(CFG_SIZE, size));
        vector<u8> data(size);
        if (size > 0) {
            EXPECT_TRUE(s.virtio_out->read_config(
                { CFG_DATA, CFG_DATA + size - 1 }, data.data()));
        }

        return data;
    }

    static bool has_bit(const vector<u8>& bitmap, size_t bit) {
        return bit / 8 < bitmap.size() &&
               (bitmap[bit / 8] & (1u << (bit % 8)));
    }

    void test_identify() {
        virtio_device_desc desc = stim.identify();
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_INPUT);
        EXPECT_EQ(desc.vendor_id, VIRTIO_VENDOR_VCML);
        EXPECT_EQ(desc.pci_class, PCI_CLASS_INPUT_KEYBOARD);
        ASSERT_EQ(desc.virtqueues.size(), 2);
        EXPECT_EQ(desc.virtqueues.at(VQ_EVENT).limit, 8);
        EXPECT_EQ(desc.virtqueues.at(VQ_STATUS).limit, 8);
        EXPECT_EQ(tstim.identify().pci_class, PCI_CLASS_INPUT_OTHER);

        EXPECT_EQ(stim.features(), 0);
        EXPECT_TRUE(stim.virtio_out->write_features(0));

        u8 data = 0;
        EXPECT_TRUE(stim.read_config(CFG_END - 1, data));
        EXPECT_FALSE(stim.read_config(CFG_END, data));
        EXPECT_FALSE(stim.write_config(CFG_SIZE, data));
        EXPECT_FALSE(stim.write_config(CFG_DATA, data));
    }

    void test_config_ids() {
        stim.identify();
        EXPECT_TRUE(select(stim, CFG_UNSET, 0).empty());

        vector<u8> name = select(stim, CFG_ID_NAME, 0);
        EXPECT_EQ(string(name.begin(), name.end()), "vcml-virtio-input");
        EXPECT_TRUE(select(stim, CFG_ID_NAME, 1).empty());

        vector<u8> serial = select(stim, CFG_ID_SERIAL, 0);
        EXPECT_EQ(string(serial.begin(), serial.end()), VCML_VERSION_STRING);
        EXPECT_TRUE(select(stim, CFG_ID_SERIAL, 1).empty());

        vector<u8> ids = select(stim, CFG_ID_DEVIDS, 0);
        EXPECT_EQ(ids, vector<u8>({ 0x06, 0x00, 0xfe, 0xca, 0x01, 0x00, 0x01,
                                    0x00 }));
        EXPECT_TRUE(select(stim, CFG_ID_DEVIDS, 1).empty());

        EXPECT_EQ(select(stim, CFG_PROP_BITS, 0), vector<u8>(128, 0));
        EXPECT_TRUE(select(stim, CFG_PROP_BITS, 1).empty());
        EXPECT_TRUE(select(stim, 0x55, 0).empty());
    }

    void test_config_events() {
        stim.identify();
        vector<u8> syn = select(stim, CFG_EV_BITS, ui::EV_SYN);
        EXPECT_EQ(syn.size(), 128);
        EXPECT_TRUE(has_bit(syn, ui::SYN_REPORT));

        vector<u8> keys = select(stim, CFG_EV_BITS, ui::EV_KEY);
        EXPECT_TRUE(has_bit(keys, ui::KEY_A));
        EXPECT_TRUE(has_bit(keys, ui::KEY_ENTER));
        EXPECT_TRUE(has_bit(keys, ui::BTN_LEFT));
        EXPECT_TRUE(has_bit(keys, ui::BTN_RIGHT));
        EXPECT_TRUE(has_bit(keys, ui::BTN_MIDDLE));
        EXPECT_FALSE(has_bit(keys, ui::BTN_TOUCH));

        vector<u8> rel = select(stim, CFG_EV_BITS, ui::EV_REL);
        EXPECT_TRUE(has_bit(rel, ui::REL_X));
        EXPECT_TRUE(has_bit(rel, ui::REL_Y));
        EXPECT_TRUE(has_bit(rel, ui::REL_WHEEL));

        EXPECT_TRUE(select(stim, CFG_EV_BITS, ui::EV_ABS).empty());
        EXPECT_TRUE(select(stim, CFG_EV_BITS, 0x15).empty());

        tstim.identify();
        keys = select(tstim, CFG_EV_BITS, ui::EV_KEY);
        EXPECT_TRUE(has_bit(keys, ui::BTN_TOUCH));
        EXPECT_TRUE(has_bit(keys, ui::BTN_TOOL_FINGER));
        EXPECT_FALSE(has_bit(keys, ui::KEY_A));
        EXPECT_FALSE(has_bit(keys, ui::BTN_LEFT));

        vector<u8> abs = select(tstim, CFG_EV_BITS, ui::EV_ABS);
        EXPECT_TRUE(has_bit(abs, ui::ABS_X));
        EXPECT_TRUE(has_bit(abs, ui::ABS_Y));
        EXPECT_FALSE(has_bit(abs, ui::ABS_MT_SLOT));
        EXPECT_TRUE(select(tstim, CFG_EV_BITS, ui::EV_REL).empty());

        mstim.identify();
        abs = select(mstim, CFG_EV_BITS, ui::EV_ABS);
        EXPECT_TRUE(has_bit(abs, ui::ABS_MT_SLOT));
        EXPECT_TRUE(has_bit(abs, ui::ABS_MT_TRACKING_ID));
        EXPECT_TRUE(has_bit(abs, ui::ABS_MT_POSITION_X));
        EXPECT_TRUE(has_bit(abs, ui::ABS_MT_POSITION_Y));
        EXPECT_FALSE(has_bit(abs, ui::ABS_X));
    }

    void test_config_absinfo() {
        struct absinfo {
            u32 min;
            u32 max;
            u32 fuzz;
            u32 flat;
            u32 res;
        };

        const std::pair<u8, u32> expected[] = {
            { ui::ABS_X, 1000 },
            { ui::ABS_Y, 500 },
            { ui::ABS_MT_POSITION_X, 1000 },
            { ui::ABS_MT_POSITION_Y, 500 },
            { ui::ABS_MT_SLOT, 4 },
        };

        tstim.identify();
        for (const auto& [axis, max] : expected) {
            vector<u8> data = select(tstim, CFG_ABS_INFO, axis);
            absinfo info = {};
            ASSERT_EQ(data.size(), sizeof(info)) << "axis " << (int)axis;
            memcpy(&info, data.data(), sizeof(info));
            EXPECT_EQ(info.min, 0) << "axis " << (int)axis;
            EXPECT_EQ(info.max, max) << "axis " << (int)axis;
        }

        EXPECT_TRUE(select(tstim, CFG_ABS_INFO, 0x20).empty());
    }

    void test_keyboard() {
        auto* kbd = ui::input::find<ui::keyboard>("stim.input.keyboard");
        ASSERT_NE(kbd, nullptr);
        prepare(stim);

        kbd->notify_key(ui::KEYSYM_a, true);
        expect_events(stim, { { ui::EV_KEY, ui::KEY_A, 1 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        kbd->notify_key(ui::KEYSYM_a, true);
        expect_events(stim, { { ui::EV_KEY, ui::KEY_A, 2 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        kbd->notify_key(ui::KEYSYM_a, false);
        expect_events(stim, { { ui::EV_KEY, ui::KEY_A, 0 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        expect_events(stim, {});
    }

    void test_mouse() {
        auto* mouse = ui::input::find<ui::mouse>("stim.input.mouse");
        ASSERT_NE(mouse, nullptr);
        prepare(stim);

        const std::pair<u32, u16> buttons[] = {
            { ui::BUTTON_LEFT, ui::BTN_LEFT },
            { ui::BUTTON_RIGHT, ui::BTN_RIGHT },
            { ui::BUTTON_MIDDLE, ui::BTN_MIDDLE },
            { ui::BUTTON_SIDE, ui::BTN_SIDE },
            { ui::BUTTON_EXTRA, ui::BTN_EXTRA },
        };

        for (const auto& [button, code] : buttons) {
            prepare(stim);
            mouse->notify_btn(button, true);
            expect_events(stim, { { ui::EV_KEY, code, 1 },
                                  { ui::EV_SYN, ui::SYN_REPORT, 0 } });
            mouse->notify_btn(button, true);
            expect_events(stim, {});
            mouse->notify_btn(button, false);
            expect_events(stim, { { ui::EV_KEY, code, 0 },
                                  { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        }

        prepare(stim);
        mouse->notify_btn(ui::BUTTON_WHEEL_UP, true);
        expect_events(stim, { { ui::EV_REL, ui::REL_WHEEL, 1 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        mouse->notify_btn(ui::BUTTON_WHEEL_UP, false);
        mouse->notify_btn(ui::BUTTON_WHEEL_DOWN, true);
        wait(sc_time(5, SC_MS));
        mouse->notify_btn(ui::BUTTON_WHEEL_DOWN, false);
        mouse->notify_btn(ui::BUTTON_WHEEL_RIGHT, true);
        wait(sc_time(5, SC_MS));
        mouse->notify_btn(ui::BUTTON_WHEEL_RIGHT, false);
        mouse->notify_btn(ui::BUTTON_WHEEL_LEFT, true);
        wait(sc_time(5, SC_MS));
        mouse->notify_btn(ui::BUTTON_WHEEL_LEFT, false);
        wait(sc_time(5, SC_MS));

        prepare(stim);
        u32 x = mouse->x();
        u32 y = mouse->y();
        mouse->notify_pos(x + 10, y + 20);
        expect_events(stim, { { ui::EV_REL, ui::REL_X, 10 },
                              { ui::EV_REL, ui::REL_Y, 20 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        mouse->notify_pos(x + 5, y + 20);
        expect_events(stim, { { ui::EV_REL, ui::REL_X, -5 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        mouse->notify_pos(x + 5, y + 20);
        expect_events(stim, {});
        mouse->notify_pos(x, y);
        mouse->notify_key(ui::KEYSYM_a, true);
        wait(sc_time(5, SC_MS));
    }

    void test_touchpad() {
        auto* pad = ui::input::find<ui::touchpad>("stim.touch.touchpad");
        ASSERT_NE(pad, nullptr);
        prepare(tstim, 16);

        pad->notify_pos(5000, 2500);
        expect_events(tstim, {});

        pad->notify_btn(ui::BUTTON_LEFT, true);
        expect_events(tstim, { { ui::EV_KEY, ui::BTN_TOUCH, 1 },
                               { ui::EV_ABS, ui::ABS_X, 500 },
                               { ui::EV_ABS, ui::ABS_Y, 125 },
                               { ui::EV_SYN, ui::SYN_REPORT, 0 } });

        pad->notify_pos(10000, 10000);
        expect_events(tstim, { { ui::EV_ABS, ui::ABS_X, 1000 },
                               { ui::EV_ABS, ui::ABS_Y, 500 },
                               { ui::EV_SYN, ui::SYN_REPORT, 0 } });

        pad->notify_pos(10000, 0);
        expect_events(tstim, { { ui::EV_ABS, ui::ABS_Y, 0 },
                               { ui::EV_SYN, ui::SYN_REPORT, 0 } });

        pad->notify_btn(ui::BUTTON_LEFT, true);
        pad->notify_key(ui::KEYSYM_a, true);
        expect_events(tstim, {});

        pad->notify_btn(ui::BUTTON_LEFT, false);
        expect_events(tstim, { { ui::EV_KEY, ui::BTN_TOUCH, 0 },
                               { ui::EV_SYN, ui::SYN_REPORT, 0 } });
        pad->notify_pos(0, 0);
    }

    void test_multitouch() {
        auto* mt = ui::input::find<ui::multitouch>("stim.mtouch.multitouch");
        ASSERT_NE(mt, nullptr);
        prepare(mstim);

        mt->notify_pos(5000, 2500);
        mt->notify_btn(ui::BUTTON_LEFT, true);
        mt->notify_key(ui::KEYSYM_a, true);
        expect_events(mstim, { { ui::EV_ABS, ui::ABS_MT_SLOT, 0 },
                               { ui::EV_ABS, ui::ABS_MT_TRACKING_ID, 0 },
                               { ui::EV_ABS, ui::ABS_MT_POSITION_X, 16383 },
                               { ui::EV_ABS, ui::ABS_MT_POSITION_Y, 8191 },
                               { ui::EV_SYN, ui::SYN_REPORT, 0 } });

        mt->notify_btn(ui::BUTTON_LEFT, false);
        wait(sc_time(5, SC_MS));
        mt->notify_pos(0, 0);
    }

    void test_backpressure() {
        auto* kbd = ui::input::find<ui::keyboard>("stim.input.keyboard");
        ASSERT_NE(kbd, nullptr);
        prepare(stim, 0);

        kbd->notify_key(ui::KEYSYM_1, true);
        kbd->notify_key(ui::KEYSYM_1, false);
        expect_events(stim, {});

        stim.submit(VQ_EVENT, {}, { sizeof(event) });
        EXPECT_TRUE(stim.kick(VQ_EVENT));
        expect_events(stim, { { ui::EV_KEY, ui::KEY_1, 1 } });

        stim.accept = false;
        for (size_t i = 0; i < 3; i++)
            stim.submit(VQ_EVENT, {}, { sizeof(event) });
        EXPECT_TRUE(stim.kick(VQ_EVENT));
        expect_events(stim, {});

        stim.accept = true;
        expect_events(stim, { { ui::EV_SYN, ui::SYN_REPORT, 0 },
                              { ui::EV_KEY, ui::KEY_1, 0 },
                              { ui::EV_SYN, ui::SYN_REPORT, 0 } });

        stim.identify();
    }
};

TEST(virtio, rng) {
    broker broker("test");
    broker.define("stim.touch.keyboard", "false");
    broker.define("stim.touch.mouse", "false");
    broker.define("stim.touch.touchpad", "true");
    broker.define("stim.touch.xmax", "1000");
    broker.define("stim.touch.ymax", "500");
    broker.define("stim.mtouch.keyboard", "false");
    broker.define("stim.mtouch.mouse", "false");
    broker.define("stim.mtouch.multitouch", "true");

    virtio_rng_stim stim("stim");
    sc_core::sc_start();
}
