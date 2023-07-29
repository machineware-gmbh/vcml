/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/opencores/ockbd.h"

#define MOD_RELEASE (1 << 7)

namespace vcml {
namespace opencores {

void ockbd::update() {
    ui::input_event event;
    while (m_keyboard.pop_event(event)) {
        VCML_ERROR_ON(!event.is_key(), "illegal event from keyboard");
        u8 scancode = (u8)(event.key.code & 0xff);
        bool down = (event.key.state != ui::VCML_KEY_UP);

        if (!down)
            scancode |= MOD_RELEASE;

        if (m_key_fifo.size() < fifosize)
            m_key_fifo.push(scancode);
        else
            log_debug("FIFO full, dropping key");
    }

    if (!irq && !m_key_fifo.empty())
        log_debug("setting IRQ");

    irq = !m_key_fifo.empty();

    sc_time quantum = tlm_global_quantum::instance().get();
    next_trigger(max(clock_cycle(), quantum));
}

u8 ockbd::read_khr() {
    VCML_ERROR_ON(irq && m_key_fifo.empty(), "IRQ without data");

    if (m_key_fifo.empty()) {
        log_debug("read KHR without data and interrupt");
        return 0;
    }

    u8 key = m_key_fifo.front();
    m_key_fifo.pop();

    log_debug("cpu fetched key 0x%hhx from KHR, %zu keys remaining", key,
              m_key_fifo.size());
    if (irq && m_key_fifo.empty())
        log_debug("clearing IRQ");

    irq = !m_key_fifo.empty();
    return key;
}

ockbd::ockbd(const sc_module_name& nm):
    peripheral(nm),
    m_key_fifo(),
    m_keyboard(name()),
    m_console(),
    khr("khr", 0x0, 0),
    irq("irq"),
    in("in"),
    keymap("keymap", "us"),
    fifosize("fifosize", 16) {
    m_keyboard.set_layout(keymap);

    khr.allow_read_only();
    khr.on_read(&ockbd::read_khr);

    if (m_console.has_display()) {
        m_console.notify(m_keyboard);
        SC_HAS_PROCESS(ockbd);
        SC_METHOD(update);
    }
}

ockbd::~ockbd() {
    // nothing to do
}

void ockbd::end_of_simulation() {
    m_console.shutdown();
    peripheral::end_of_simulation();
}

VCML_EXPORT_MODEL(vcml::opencores::ockbd, name, args) {
    return new ockbd(name);
}

} // namespace opencores
} // namespace vcml
