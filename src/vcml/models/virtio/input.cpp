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
        switch (m_config.subsel) {
        case EV_SYN:
            m_config.u.bitmap[0] |= 1u << SYN_REPORT;
            m_config.size = 1;
            break;

        case EV_KEY:
            if (keyboard) {
                auto keys = ui::keymap::lookup(keymap);
                for (auto k : keys.layout) {
                    if (k.code < sizeof(m_config.u.bitmap) * 8)
                        m_config.u.bitmap[k.code / 8] |= 1u << (k.code % 8);
                }
                m_config.size = sizeof(m_config.u.bitmap);
            }

            break;

        default:
            // ignore the other event types
            break;
        }
    }

    void input::config_update_absinfo() {
        if (vncport == 0 || !touchpad)
            return;

        auto vnc = ui::vnc::lookup(vncport);

        switch (m_config.subsel) {
        case ABS_X:
            m_config.u.abs.min  = 0;
            m_config.u.abs.max  = vnc->resx();
            m_config.size = sizeof(m_config.u.abs);
            break;

        case ABS_Y:
            m_config.u.abs.min  = 0;
            m_config.u.abs.max  = vnc->resy();
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

    void input::key_event(u32 key, bool down) {
        const auto& map = ui::keymap::lookup(keymap);
        auto info = map.lookup_symbol(key);

        if (map.is_reserved(info))
            return;

        u32 val = 0u;
        if (down) { // handle up 0, down 1, repeat 2
            val = (key == m_prev_symbol) ? 2u : 1u;
            m_prev_symbol = key;
        }

        lock_guard<mutex> lock(m_events_mutex);
        if (info->shift)
            m_events.push({EV_KEY, KEY_LEFTSHIFT, (u32)down});
        if (info->l_alt)
            m_events.push({EV_KEY, KEY_LEFTALT, (u32)down});
        if (info->r_alt)
            m_events.push({EV_KEY, KEY_RIGHTALT, (u32)down});

        m_events.push({EV_KEY, info->code, val});
        m_events.push({EV_SYN, SYN_REPORT, 0u});
    }

    void input::update() {
        lock_guard<mutex> lock(m_events_mutex);

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
        m_key_handler(),
        m_prev_symbol(),
        mouse("mouse", true),
        touchpad("touchpad", true),
        keyboard("keyboard", true),
        keymap("keymap", "us"),
        pollrate("pollrate", 1000),
        vncport("vncport", 0),
        VIRTIO_IN("VIRTIO_IN") {
        VIRTIO_IN.bind(*this);

        using std::placeholders::_1;
        using std::placeholders::_2;
        m_key_handler = std::bind(&input::key_event, this, _1, _2);

        if (vncport > 0) {
            auto vnc = ui::vnc::lookup(vncport);
            if (keyboard)
                vnc->add_key_listener(&m_key_handler);
        }

        if (mouse || keyboard || keyboard) {
            SC_HAS_PROCESS(input);
            SC_METHOD(update);
        }
    }

    input::~input() {
        if (vncport > 0) {
            auto vnc = ui::vnc::lookup(vncport);
            if (keyboard)
                vnc->remove_key_listener(&m_key_handler);
        }
    }

    void input::reset() {
        memset(&m_config, 0, sizeof(m_config));

        m_messages = {};
        m_events = {};
    }

}}
