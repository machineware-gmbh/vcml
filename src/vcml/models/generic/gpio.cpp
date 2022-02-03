/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#include "vcml/models/generic/gpio.h"

namespace vcml { namespace generic {

    bool gpio::cmd_status(const vector<string>& args, ostream& os) {
        os << basename() << " status" << std::endl
           << "  DATA: 0x" << std::hex << std::setw(8) << std::setfill('0')
           << read_DATA() << std::dec << std::setfill(' ') << std::endl;

        os << "Set: ";
        for (auto port : GPIO) {
            if (port.second->read())
                os << port.first << ", ";
        }
        os << std::endl;

        os << "Cleared: ";
        for (auto port : GPIO) {
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

        if (!GPIO.exists(idx)) {
            os << "GPIO" << idx << " not connected";
            return false;
        }

        os << "setting GPIO" << idx;
        GPIO[idx].write(true);
        return true;
    }

    bool gpio::cmd_clear(const vector<string>& args, ostream& os) {
        unsigned long long idx = strtoull(args[0].c_str(), NULL, 0);
        if (idx > 31) {
            os << "index out of bounds: " << idx;
            return false;
        }

        if (!GPIO.exists(idx)) {
            os << "GPIO" << idx << " not connected";
            return false;
        }

        os << "clearing GPIO" << idx;
        GPIO[idx].write(false);
        return true;
    }

    u32 gpio::read_DATA() {
        u32 result = 0;
        for (auto gpio : GPIO) {
            VCML_ERROR_ON(gpio.first > 31, "invalid GPIO%u", gpio.first);

            if (gpio.second->read())
                result |= (1 << gpio.first);
        }

        return result;
    }

    void gpio::write_DATA(u32 val) {
        for (auto gpio : GPIO) {
            VCML_ERROR_ON(gpio.first > 31, "invalid GPIO%u", gpio.first);

            u32 mask = (1 << gpio.first);
            if ((val & mask) && !gpio.second->read()) {
                log_debug("setting GPIO%u", gpio.first);
                gpio.second->write(true);
            }

            if (!(val & mask) && gpio.second->read()) {
                log_debug("clearing GPIO%u", gpio.first);
                gpio.second->write(false);
            }
        }

        DATA = val;
    }

    gpio::gpio(const sc_module_name& nm):
        peripheral(nm),
        DATA("DATA", 0x0, 0),
        GPIO("PORTS"),
        IN("IN") {
        DATA.allow_read_write();
        DATA.on_read(&gpio::read_DATA);
        DATA.on_write(&gpio::write_DATA);

        register_command("status", 0, this, &gpio::cmd_status,
                         "reports the status of all GPIO lines");
        register_command("set", 1, this, &gpio::cmd_set,
                         "sets the given GPIO line");
        register_command("clear", 1, this, &gpio::cmd_clear,
                         "clears the given GPIO line");
    }

    gpio::~gpio() {
        // nothing to do
    }

    void gpio::reset() {
        peripheral::reset();
        write_DATA(0);
    }

    void gpio::end_of_elaboration() {
        peripheral::end_of_elaboration();

        bool valid_binding = true;
        for (auto port : GPIO) {
            if (port.first > 31) {
                log_warn("GPIO index out of bounds: %u", port.first);
                valid_binding = false;
            }
        }

        VCML_ERROR_ON(!valid_binding, "invalid port binding");
    }

}}
