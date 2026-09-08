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

static bool released_state(const buttons& btn) {
    return !btn.pressed_state;
}

static bool find_button(gpio_initiator_array<>& gpio_in, const string& arg,
                        size_t& idx, ostream& os) {
    idx = from_string<size_t>(arg);
    if (!gpio_in.exists(idx)) {
        os << "button" << idx << " not connected";
        return false;
    }

    return true;
}

bool buttons::cmd_push(const vector<string>& args, ostream& os) {
    size_t idx;
    if (!find_button(gpio_in, args[0], idx, os))
        return false;

    gpio_in[idx] = pressed_state;
    os << "button" << idx << " pressed";
    return true;
}

bool buttons::cmd_release(const vector<string>& args, ostream& os) {
    size_t idx;
    if (!find_button(gpio_in, args[0], idx, os))
        return false;

    gpio_in[idx] = released_state(*this);
    os << "button" << idx << " released";
    return true;
}

bool buttons::cmd_pulse(const vector<string>& args, ostream& os) {
    size_t idx;
    if (!find_button(gpio_in, args[0], idx, os))
        return false;

    gpio_in[idx] = pressed_state;
    gpio_in[idx] = released_state(*this);
    os << "button" << idx << " pulsed";
    return true;
}

buttons::buttons(const sc_module_name& nm):
    module(nm),
    gpio_host(),
    pressed_state("pressed_state", true),
    gpio_in("gpio_in") {
    register_command("push", 1, &buttons::cmd_push,
                     "presses the given button");
    register_command("release", 1, &buttons::cmd_release,
                     "releases the given button");
    register_command("pulse", 1, &buttons::cmd_pulse,
                     "presses and releases the given button");
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
