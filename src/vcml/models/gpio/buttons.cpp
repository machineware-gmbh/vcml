/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/gpio/buttons.h"

namespace vcml {
namespace gpio {

bool buttons::cmd_status(const vector<string>& args, ostream& os) {
    if (gpio_in.count() == 0) {
        os << "no buttons connected" << std::endl;
        return true;
    }

    for (auto [id, button] : gpio_in) {
        bool pressed = button->read() == pressed_state;
        os << "BUTTON" << id << ": " << (pressed ? "pressed" : "released")
           << std::endl;
    }

    return true;
}

static optional<size_t> find_button(gpio_initiator_array<>& gpio_in,
                                    const string& arg, ostream& os) {
    size_t idx = from_string<size_t>(arg);
    if (!gpio_in.exists(idx)) {
        os << "button" << idx << " not connected";
        return {};
    }

    return idx;
}

bool buttons::cmd_push(const vector<string>& args, ostream& os) {
    lock_guard<mutex> guard(m_cmd_mutex);
    auto idx = find_button(gpio_in, args[0], os);
    if (!idx)
        return false;

    m_cmd_functions.push_back(
        [this, idx]() { gpio_in[*idx] = pressed_state; });
    on_next_update([this]() { m_cmd_event.notify(SC_ZERO_TIME); });

    os << "button" << *idx << " pressed";
    return true;
}

bool buttons::cmd_release(const vector<string>& args, ostream& os) {
    lock_guard<mutex> guard(m_cmd_mutex);
    auto idx = find_button(gpio_in, args[0], os);
    if (!idx)
        return false;

    m_cmd_functions.push_back(
        [this, idx]() { gpio_in[*idx] = !pressed_state; });
    on_next_update([this]() { m_cmd_event.notify(SC_ZERO_TIME); });

    os << "button" << *idx << " released";
    return true;
}

bool buttons::cmd_pulse(const vector<string>& args, ostream& os) {
    lock_guard<mutex> guard(m_cmd_mutex);
    auto idx = find_button(gpio_in, args[0], os);
    if (!idx)
        return false;

    m_cmd_functions.push_back([this, idx]() {
        gpio_in[*idx] = pressed_state;
        gpio_in[*idx] = !pressed_state;
    });

    on_next_update([this]() { m_cmd_event.notify(SC_ZERO_TIME); });

    os << "button" << *idx << " pulsed";
    return true;
}

void buttons::exec_cmd() {
    lock_guard<std::mutex> guard(m_cmd_mutex);
    for (auto& cmd : m_cmd_functions)
        cmd();
    m_cmd_functions.clear();
}

buttons::buttons(const sc_module_name& nm):
    module(nm),
    gpio_host(),
    pressed_state("pressed_state", true),
    gpio_in("gpio_in") {
    register_command("status", 0, &buttons::cmd_status,
                     "reports the status of all connected buttons");
    register_command("push", 1, &buttons::cmd_push,
                     "presses the given button");
    register_command("release", 1, &buttons::cmd_release,
                     "releases the given button");
    register_command("pulse", 1, &buttons::cmd_pulse,
                     "presses and releases the given button");
    SC_METHOD(exec_cmd);
    sensitive << m_cmd_event;
    dont_initialize();
}

void buttons::gpio_transport(const gpio_target_socket& socket,
                             gpio_payload& tx) {
    (void)socket;
    (void)tx;
}

VCML_EXPORT_MODEL(vcml::gpio::buttons, name, args) {
    return new buttons(name);
}

} // namespace gpio
} // namespace vcml
