/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/input.h"

namespace vcml { namespace virtio {

    void input::config_update_name() {
        if (m_config.subsel)
            return;

        m_config.size = snprintf(m_config.u.string, sizeof(m_config.u.string),
                                 "virtio input device");
    }

    void input::config_update_serial() {
        if (m_config.subsel)
            return;

        m_config.size = snprintf(m_config.u.string, sizeof(m_config.u.string),
                                 "1234567890");
    }

    void input::config_update_devids() {
        if (m_config.subsel)
            return;

        m_config.u.ids.bustype = 1;
        m_config.u.ids.vendor  = 2;
        m_config.u.ids.product = 3;
        m_config.u.ids.version = 4;
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
        case EV_SYN:
            events.set(SYN_REPORT);
            break;

        case EV_KEY:
            if (keyboard) {
                auto keys = ui::keymap::lookup(keymap);
                for (auto key : keys.layout)
                    events.set(key.code);
            }

            if (touchpad) {
                events.set(BTN_TOUCH);
                events.set(BTN_TOOL_FINGER);
                events.set(BTN_TOOL_DOUBLETAP);
                events.set(BTN_TOOL_TRIPLETAP);
            }

            break;

        case EV_ABS:
            if (touchpad) {
                events.set(ABS_X);
                events.set(ABS_Y);
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
        if (display == "" || !touchpad)
            return;

        auto disp = ui::display::lookup(display);

        switch (m_config.subsel) {
        case ABS_X:
            m_config.u.abs.min  = 0;
            m_config.u.abs.max  = disp->resx() - 1;
            m_config.size = sizeof(m_config.u.abs);
            break;

        case ABS_Y:
            m_config.u.abs.min  = 0;
            m_config.u.abs.max  = disp->resy() - 1;
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
        ui::input_event event = {};
        while (m_keyboard.pop_event(event)) {
            push_key(event.key.code, event.key.state);
            push_sync();
        }

        while (m_pointer.pop_event(event)) {
            if (event.is_key()) {
                push_key(event.key.code, event.key.state);
                push_sync();
            } else if (event.is_ptr()) {
                push_abs(ABS_X, event.ptr.x);
                push_abs(ABS_Y, event.ptr.y);
                push_sync();
            }
        }

        if (!m_events.empty() && !m_messages.empty()) {
            vq_message msg(m_messages.front());
            input_event event(m_events.front());

            msg.copy_out(event);

            if (event.type == EV_SYN && event.code == SYN_REPORT) {
                log_debug("event sync");
            } else {
                log_debug("event type %hu, code %hu, value %u",
                          event.type, event.code, event.value);
            }

            if (VIRTIO_IN->put(VIRTQUEUE_EVENT, msg)) {
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
        desc.request_virtqueue(VIRTQUEUE_EVENT, 8);
        desc.request_virtqueue(VIRTQUEUE_STATUS, 8);
    }

    bool input::notify(u32 vqid) {
        vq_message msg;
        while (VIRTIO_IN->get(vqid, msg))
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
        m_config(),
        m_keyboard(name()),
        m_pointer(name()),
        touchpad("touchpad", true),
        keyboard("keyboard", true),
        pollrate("pollrate", 1000),
        keymap("keymap", "us"),
        display("display", ""),
        VIRTIO_IN("VIRTIO_IN") {
        VIRTIO_IN.bind(*this);

        m_keyboard.set_layout(keymap);

        if (display != "") {
            auto disp = ui::display::lookup(display);
            if (keyboard)
                disp->add_keyboard(&m_keyboard);
            if (touchpad)
                disp->add_pointer(&m_pointer);
        }

        if (keyboard || touchpad) {
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
        if (display != "") {
            auto disp = ui::display::lookup(display);
            if (keyboard)
                disp->remove_keyboard(&m_keyboard);
            if (touchpad)
                disp->remove_pointer(&m_pointer);
            disp->shutdown();
        }
    }

}}
