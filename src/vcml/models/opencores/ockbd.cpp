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

namespace vcml { namespace opencores {

    SC_HAS_PROCESS(ockbd);

    void ockbd::update() {
        u8 key = 0;
        if (m_vnc != NULL) {
            key = 0; // TODO, ask vncserver for key input
            if (key != 0)
                log_debug("got key %d from vncserver", (int)key);
        } else {
            beread(key);
            if (key != 0)
                log_debug("got key %d from backend", (int)key);
        }

        if (key != 0) {
            if (m_key_fifo.size() < m_key_size)
                m_key_fifo.push(key);
            else
                log_debug("FIFO full, dropping key %d", (int)key);
        }

        IRQ = !m_key_fifo.empty();
    }

    void ockbd::poll() {
        update();

        // it does not make sense to poll multiple times during
        // a quantum, so if a quantum is set, only update once.
        sc_time cycle = sc_time(1.0 / clock, SC_SEC);
        sc_time quantum = tlm_global_quantum::instance().get();
        next_trigger(max(cycle, quantum));
    }

    u8 ockbd::read_KHR() {
        VCML_ERROR_ON(IRQ && m_key_fifo.empty(), "IRQ without data");

        if (m_key_fifo.empty())
            return 0;

        u8 key = m_key_fifo.front();
        m_key_fifo.pop();
        IRQ = !m_key_fifo.empty();
        return key;
    }

    ockbd::ockbd(const sc_module_name& nm):
        peripheral(nm),
        m_key_size(16),
        m_key_fifo(),
        m_vnc(),
        KHR("KHR", 0x0, 0),
        IRQ("IRQ"),
        IN("IN"),
        clock("clock", 20000000), // 20 MHz
        vncport("vncport", 0) {
        if (vncport > 0)
            m_vnc = debugging::vncserver::lookup(vncport);
        if (m_vnc != NULL)
            log_debug("using vncserver at port %d", (int)m_vnc->get_port());

        KHR.allow_read();
        KHR.read = &ockbd::read_KHR;

        SC_METHOD(poll);
    }

    ockbd::~ockbd() {
        /* nothing to do */
    }

}}
