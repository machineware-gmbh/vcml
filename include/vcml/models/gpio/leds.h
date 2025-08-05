/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GPIO_LEDS_H
#define VCML_GPIO_LEDS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/gpio.h"

namespace vcml {
namespace gpio {

class leds : public module, public gpio_host
{
public:
    gpio_target_array<> gpio_in;

    leds(const sc_module_name& name);
    virtual ~leds() = default;
    VCML_KIND(gpio::leds);

protected:
    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;
};

} // namespace gpio
} // namespace vcml

#endif
