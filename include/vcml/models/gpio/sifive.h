/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GPIO_SIFIVE_H
#define VCML_GPIO_SIFIVE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace gpio {

class sifive : public peripheral
{
private:
    struct gpio_info {
        bool is_output;
        bool is_input;
        bool prev_val;
        bool curr_val;
    } m_gpios[32];

    bool cmd_status(const vector<string>& args, ostream& os);
    bool cmd_set(const vector<string>& args, ostream& os);
    bool cmd_clear(const vector<string>& args, ostream& os);

    void update();
    void update_irq();

    u32 read_input_val();

    void write_input_en(u32 val);
    void write_output_en(u32 val);
    void write_output_val(u32 val);
    void write_rise_ie(u32 val);
    void write_rise_ip(u32 val);
    void write_fall_ie(u32 val);
    void write_fall_ip(u32 val);
    void write_high_ie(u32 val);
    void write_high_ip(u32 val);
    void write_low_ie(u32 val);
    void write_low_ip(u32 val);
    void write_out_xor(u32 val);

public:
    property<size_t> ngpios;

    reg<u32> input_val;
    reg<u32> input_en;
    reg<u32> output_en;
    reg<u32> output_val;
    reg<u32> pue;
    reg<u32> ds;
    reg<u32> rise_ie;
    reg<u32> rise_ip;
    reg<u32> fall_ie;
    reg<u32> fall_ip;
    reg<u32> high_ie;
    reg<u32> high_ip;
    reg<u32> low_ie;
    reg<u32> low_ip;
    reg<u32> out_xor;

    gpio_initiator_array irq;
    gpio_initiator_array gpio_out;
    gpio_target_array gpio_in;
    tlm_target_socket in;

    sifive(const sc_module_name& name, size_t ngpios = 16);
    virtual ~sifive() = default;
    VCML_KIND(gpio::sifive);
    virtual void reset() override;

protected:
    virtual void gpio_notify(const gpio_target_socket& socket) override;
    virtual void end_of_elaboration() override;
};

} // namespace gpio
} // namespace vcml

#endif
