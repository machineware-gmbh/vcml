/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/gpio.h"

namespace vcml {
namespace generic {

bool gpio::cmd_status(const vector<string>& args, ostream& os) {
    os << basename() << " status" << std::endl
       << "  DATA: 0x" << std::hex << std::setw(8) << std::setfill('0')
       << read_data() << std::dec << std::setfill(' ') << std::endl;

    os << "Set: ";
    for (auto port : gpio_out) {
        if (port.second->read())
            os << port.first << ", ";
    }
    os << std::endl;

    os << "Cleared: ";
    for (auto port : gpio_out) {
        if (!port.second->read())
            os << port.first << ", ";
    }
    os << std::endl;

    return true;
}

bool gpio::cmd_set(const vector<string>& args, ostream& os) {
    unsigned long long idx = strtoull(args[0].c_str(), NULL, 0);
    if (idx > 31) {
        os << "index out of bounds: " << idx;
        return false;
    }

    if (!gpio_out.exists(idx)) {
        os << "GPIO" << idx << " not connected";
        return false;
    }

    os << "setting GPIO" << idx;
    gpio_out[idx] = true;
    return true;
}

bool gpio::cmd_clear(const vector<string>& args, ostream& os) {
    unsigned long long idx = strtoull(args[0].c_str(), NULL, 0);
    if (idx > 31) {
        os << "index out of bounds: " << idx;
        return false;
    }

    if (!gpio_out.exists(idx)) {
        os << "GPIO" << idx << " not connected";
        return false;
    }

    os << "clearing GPIO" << idx;
    gpio_out[idx] = false;
    return true;
}

u32 gpio::read_data() {
    u32 result = 0;
    for (auto gpio : gpio_out) {
        VCML_ERROR_ON(gpio.first > 31, "invalid GPIO%lu", gpio.first);

        if (gpio.second->read())
            result |= (1 << gpio.first);
    }

    return result;
}

void gpio::write_data(u32 val) {
    for (auto port : gpio_out) {
        VCML_ERROR_ON(port.first > 31, "invalid GPIO%lu", port.first);

        u32 mask = (1 << port.first);
        if ((val & mask) && !port.second->read()) {
            log_debug("setting GPIO%lu", port.first);
            *port.second = true;
        }

        if (!(val & mask) && port.second->read()) {
            log_debug("clearing GPIO%lu", port.first);
            *port.second = false;
        }
    }

    data = val;
}

gpio::gpio(const sc_module_name& nm):
    peripheral(nm), data("data", 0x0, 0), gpio_out("gpio_out"), in("in") {
    data.allow_read_write();
    data.on_read(&gpio::read_data);
    data.on_write(&gpio::write_data);

    register_command("status", 0, &gpio::cmd_status,
                     "reports the status of all GPIO lines");
    register_command("set", 1, &gpio::cmd_set, "sets the given GPIO line");
    register_command("clear", 1, &gpio::cmd_clear,
                     "clears the given GPIO line");
}

gpio::~gpio() {
    // nothing to do
}

void gpio::reset() {
    peripheral::reset();
    write_data(0);
}

void gpio::end_of_elaboration() {
    peripheral::end_of_elaboration();

    bool valid_binding = true;
    for (auto port : gpio_out) {
        if (port.first > 31) {
            log_warn("GPIO index out of bounds: %lu", port.first);
            valid_binding = false;
        }
    }

    VCML_ERROR_ON(!valid_binding, "invalid port binding");
}

VCML_EXPORT_MODEL(vcml::generic::gpio, name, args) {
    return new gpio(name);
}

} // namespace generic
} // namespace vcml
