/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GPIO_MMGPIO_H
#define VCML_GPIO_MMGPIO_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace gpio {

class mmgpio : public peripheral
{
private:
    bool cmd_status(const vector<string>& args, ostream& os);
    bool cmd_set(const vector<string>& args, ostream& os);
    bool cmd_clear(const vector<string>& args, ostream& os);

    u32 read_data();
    void write_data(u32 val);

public:
    reg<u32> data;

    gpio_initiator_array gpio_out;
    tlm_target_socket in;

    mmgpio() = delete;
    mmgpio(const sc_module_name& name);
    virtual ~mmgpio();
    VCML_KIND(gpio::mmgpio);
    virtual void reset() override;

    virtual void end_of_elaboration() override;
};

} // namespace gpio
} // namespace vcml

#endif
