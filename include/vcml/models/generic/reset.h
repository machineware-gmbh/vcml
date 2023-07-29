/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_RESET_H
#define VCML_GENERIC_RESET_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/gpio.h"

namespace vcml {
namespace generic {

class reset : public module
{
public:
    property<bool> state;

    gpio_initiator_socket rst;

    reset() = delete;
    reset(const sc_module_name& nm, bool init_state = false);
    virtual ~reset();
    VCML_KIND(reset);

protected:
    virtual void end_of_elaboration() override;
};

} // namespace generic
} // namespace vcml

#endif
