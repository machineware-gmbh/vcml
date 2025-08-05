/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/gpio/leds.h"

namespace vcml {
namespace gpio {

leds::leds(const sc_module_name& nm): module(nm), gpio_in("gpio_in") {
    // nothing to do
}

void leds::gpio_transport(const gpio_target_socket& socket, gpio_payload& tx) {
    size_t idx = gpio_in.index_of(socket);
    log_info("LED%zu switched %s", idx, tx.state ? "on" : "off");
}

VCML_EXPORT_MODEL(vcml::gpio::leds, name, args) {
    return new leds(name);
}

} // namespace gpio
} // namespace vcml
