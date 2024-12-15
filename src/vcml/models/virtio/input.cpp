/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/input.h"
#include "vcml/core/version.h"

namespace vcml {
namespace virtio {

void input::config_update_name() {
    if (m_config.subsel)
        return;

    m_config.size = snprintf(m_config.u.string, sizeof(m_config.u.string),
                             "vcml-virtio-input");
}

void input::config_update_serial() {
    if (m_config.subsel)
        return;

    m_config.size = snprintf(m_config.u.string, sizeof(m_config.u.string),
                             VCML_VERSION_STRING);
}

void input::config_update_devids() {
    if (m_config.subsel)
        return;

    m_config.u.ids.bustype = 0x6; // BUS_VIRTUAL
    m_config.u.ids.vendor = 0xcafe;
    m_config.u.ids.product = 0x0001;
    m_config.u.ids.version = 1;
    m_config.size = sizeof(m_config.u.ids);
}

void input::config_update_props() {
    if (m_config.subsel)
        return;

    m_config.size = sizeof(m_config.u.bitmap);
}

void input::config_update_evbits() {
    bitset<1024> events;

    switch (m_config.subsel) {
    case ui::EV_SYN:
        events.set(ui::SYN_REPORT);
        break;

    case ui::EV_KEY:
        if (keyboard) {
            auto keys = ui::keymap::lookup(keymap);
            for (auto key : keys.layout)
                events.set(key.code);
        }

        if (touchpad) {
            events.set(ui::BTN_TOUCH);
            events.set(ui::BTN_TOOL_FINGER);
        }

        if (mouse) {
            events.set(ui::BTN_LEFT);
            events.set(ui::BTN_RIGHT);
            events.set(ui::BTN_MIDDLE);
            events.set(ui::BTN_SIDE);
            events.set(ui::BTN_EXTRA);
        }

        break;

    case ui::EV_ABS:
        if (touchpad) {
            events.set(ui::ABS_X);
            events.set(ui::ABS_Y);
        }

        if (multitouch) {
            events.set(ui::ABS_MT_TRACKING_ID);
            events.set(ui::ABS_MT_POSITION_X);
            events.set(ui::ABS_MT_POSITION_Y);
            events.set(ui::ABS_MT_SLOT);
        }

        break;

    case ui::EV_REL:
        if (mouse) {
            events.set(ui::REL_X);
            events.set(ui::REL_Y);
            events.set(ui::REL_WHEEL);
            events.set(ui::REL_HWHEEL);
        }

        break;

    default:
        // ignore the other event types
        break;
    }

    if (events.none())
        return;

    for (size_t bit = 0; bit < events.size(); bit++) {
        if (events[bit])
            m_config.u.bitmap[bit / 8] |= 1u << (bit % 8);
    }

    m_config.size = sizeof(m_config.u.bitmap);
}

void input::config_update_absinfo() {
    switch (m_config.subsel) {
    case ui::ABS_X:
    case ui::ABS_MT_POSITION_X:
        m_config.u.abs.min = 0;
        m_config.u.abs.max = xmax;
        m_config.size = sizeof(m_config.u.abs);
        break;

    case ui::ABS_Y:
    case ui::ABS_MT_POSITION_Y:
        m_config.u.abs.min = 0;
        m_config.u.abs.max = ymax;
        m_config.size = sizeof(m_config.u.abs);
        break;

    case ui::ABS_MT_SLOT:
        m_config.u.abs.min = 0;
        m_config.u.abs.max = 1;
        m_config.size = sizeof(m_config.u.abs);
        break;

    default:
        break;
    }
}

void input::config_update() {
    m_config.size = 0;
    memset(&m_config.u, 0, sizeof(m_config.u));

    switch (m_config.select) {
    case VIRTIO_INPUT_CFG_UNSET:
        break;

    case VIRTIO_INPUT_CFG_ID_NAME:
        config_update_name();
        break;

    case VIRTIO_INPUT_CFG_ID_SERIAL:
        config_update_serial();
        break;

    case VIRTIO_INPUT_CFG_ID_DEVIDS:
        config_update_devids();
        break;

    case VIRTIO_INPUT_CFG_PROP_BITS:
        config_update_props();
        break;

    case VIRTIO_INPUT_CFG_EV_BITS:
        config_update_evbits();
        break;

    case VIRTIO_INPUT_CFG_ABS_INFO:
        config_update_absinfo();
        break;

    default:
        log_warn("illegal config selection: %d", (int)m_config.select);
        break;
    }
}

void input::update() {
    size_t n = m_events.size();
    ui::input_event event = {};

    while (m_keyboard.pop_event(event) && !event.is_syn())
        m_events.push(event);

    while (m_mouse.pop_event(event) && !event.is_syn())
        m_events.push(event);

    while (m_touchpad.pop_event(event) && !event.is_syn()) {
        if (event.is_abs() && event.code == ui::ABS_X)
            event.state = (event.state * xmax) / m_touchpad.xmax();
        if (event.is_abs() && event.code == ui::ABS_Y)
            event.state = (event.state * ymax) / m_touchpad.ymax();
        m_events.push(event);
    }

    while (m_multitouch.pop_event(event) && !event.is_syn()) {
        if (event.is_abs() && event.code == ui::ABS_MT_POSITION_X)
            event.state = (event.state * xmax) / m_multitouch.xmax();
        if (event.is_abs() && event.code == ui::ABS_MT_POSITION_Y)
            event.state = (event.state * ymax) / m_multitouch.ymax();
        m_events.push(event);
    }

    if (m_events.size() > n)
        m_events.push({ ui::EV_SYN, ui::SYN_REPORT, 0 });

    while (!m_events.empty() && !m_messages.empty()) {
        vq_message msg(m_messages.front());
        ui::input_event event(m_events.front());

        msg.copy_out(event);

        if (event.type == ui::EV_SYN && event.code == ui::SYN_REPORT) {
            log_debug("event sync");
        } else {
            log_debug("event type %hu, code %hu, value %u", event.type,
                      event.code, event.state);
        }

        if (virtio_in->put(VIRTQUEUE_EVENT, msg)) {
            m_events.pop();
            m_messages.pop();
        }
    }

    sc_time quantum = tlm_global_quantum::instance().get();
    sc_time polldelay = sc_time(1.0 / pollrate, SC_SEC);
    next_trigger(max(polldelay, quantum));
}

void input::identify(virtio_device_desc& desc) {
    reset();

    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.device_id = VIRTIO_DEVICE_INPUT;
    desc.pci_class = keyboard ? PCI_CLASS_INPUT_KEYBOARD
                              : PCI_CLASS_INPUT_OTHER;
    desc.request_virtqueue(VIRTQUEUE_EVENT, 8);
    desc.request_virtqueue(VIRTQUEUE_STATUS, 8);
}

bool input::notify(u32 vqid) {
    vq_message msg;
    while (virtio_in->get(vqid, msg))
        m_messages.push(msg);
    return true;
}

void input::read_features(u64& features) {
    features = 0;
}

bool input::write_features(u64 features) {
    return true;
}

bool input::read_config(const range& addr, void* ptr) {
    if (addr.end >= sizeof(m_config))
        return false;

    memcpy(ptr, (u8*)&m_config + addr.start, addr.length());
    return true;
}

bool input::write_config(const range& addr, const void* ptr) {
    if (addr.end >= offsetof(input_config, size))
        return false;

    memcpy((u8*)&m_config + addr.start, ptr, addr.length());
    config_update();
    return true;
}

input::input(const sc_module_name& nm):
    module(nm),
    virtio_device(),
    m_config(),
    m_keyboard(mkstr("%s_keyboard", name())),
    m_touchpad(mkstr("%s_touchpad", name())),
    m_mouse(mkstr("%s_mouse", name())),
    m_multitouch(mkstr("%s_multitouch", name())),
    m_console(),
    touchpad("touchpad", false),
    keyboard("keyboard", true),
    mouse("mouse", true),
    multitouch("multitouch", false),
    pollrate("pollrate", 1000),
    keymap("keymap", "us"),
    xmax("xmax", 0x7fff),
    ymax("ymax", 0x7fff),
    virtio_in("virtio_in") {
    if (keyboard) {
        m_keyboard.set_layout(keymap);
        m_console.notify(m_keyboard);
    }

    if (touchpad)
        m_console.notify(m_touchpad);

    if (mouse)
        m_console.notify(m_mouse);

    if (multitouch)
        m_console.notify(m_multitouch);

    if (keyboard || touchpad || mouse) {
        SC_HAS_PROCESS(input);
        SC_METHOD(update);
    }
}

input::~input() {
    // nothing to do
}

void input::reset() {
    memset(&m_config, 0, sizeof(m_config));
    m_messages = {};
    m_events = {};
}

void input::end_of_simulation() {
    module::end_of_simulation();
    m_console.shutdown();
}

VCML_EXPORT_MODEL(vcml::virtio::input, name, args) {
    return new input(name);
}

} // namespace virtio
} // namespace vcml
