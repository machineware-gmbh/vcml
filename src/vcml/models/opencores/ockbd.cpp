/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/models/opencores/ockbd.h"

#define MOD_RELEASE (1 << 7)

namespace vcml { namespace opencores {

    void ockbd::key_event(u32 key, bool down) {
        const auto&  map = ui::keymap::lookup(keymap);
        auto* info = map.lookup_symbol(key);
        auto  keys = map.translate_symbol(key);

        if (info == nullptr) {
            if (down)
                log_debug("no scancode for key 0x%x", key, key);
            return;
        }

        if (down)
            log_debug("found scancode for key 0x%x: %s", key, info->name);

        for (auto key : keys)
            push_key(key, down);
    }

    void ockbd::push_key(u8 scancode, bool down) {
        if (!down)
            scancode |= MOD_RELEASE;

        thctl_enter_critical();

        if (m_key_fifo.size() < fifosize)
            m_key_fifo.push(scancode);
        else if (down)
            log_debug("FIFO full, dropping key 0x%x", scancode);

        if (!IRQ && !m_key_fifo.empty())
            log_debug("setting IRQ");

        IRQ = !m_key_fifo.empty();

        thctl_exit_critical();
    }

    u8 ockbd::read_KHR() {
        VCML_ERROR_ON(IRQ && m_key_fifo.empty(), "IRQ without data");

        if (m_key_fifo.empty()) {
            log_debug("read KHR without data and interrupt");
            return 0;
        }

        u8 key = m_key_fifo.front();
        m_key_fifo.pop();

        log_debug("cpu fetched key 0x%x from KHR, %d keys remaining",
                  (int)key, (int)m_key_fifo.size());
        if (IRQ && m_key_fifo.empty())
            log_debug("clearing IRQ");

        IRQ = !m_key_fifo.empty();
        return key;
    }

    ockbd::ockbd(const sc_module_name& nm):
        peripheral(nm),
        m_key_fifo(),
        m_key_handler(),
        KHR("KHR", 0x0, 0),
        IRQ("IRQ"),
        IN("IN"),
        keymap("keymap", "us"),
        fifosize("fifosize", 16),
        vncport("vncport", 0) {

        KHR.allow_read_only();
        KHR.read = &ockbd::read_KHR;

        using std::placeholders::_1;
        using std::placeholders::_2;
        m_key_handler = std::bind(&ockbd::key_event, this, _1, _2);

        if (vncport > 0) {
            auto vnc = ui::vnc::lookup(vncport);
            vnc->add_key_listener(m_key_handler);
        }
    }

    ockbd::~ockbd() {
        // nothing to do
    }

    void ockbd::end_of_simulation() {
        if (vncport > 0) {
            auto vnc = ui::vnc::lookup(vncport);
            vnc->remove_key_listener(m_key_handler);
            vnc->shutdown();
        }
    }

}}
