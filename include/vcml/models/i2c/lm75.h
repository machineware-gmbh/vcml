/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_MODELS_LM75_H
#define VCML_MODELS_LM75_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/gpio.h"
#include "vcml/protocols/i2c.h"

namespace vcml {
namespace i2c {

class lm75 : public module, public i2c_host
{
private:
    u8 m_buf[2];
    size_t m_len;
    bool m_evt;

    bool cmd_set_temp(const vector<string>& args, ostream& os);
    bool cmd_set_high(const vector<string>& args, ostream& os);
    bool cmd_set_hyst(const vector<string>& args, ostream& os);

    void poll_temp();
    void irq_update();
    void load_buffer();
    void save_buffer();

public:
    enum reg_address : u8 {
        REG_TEMP = 0,
        REG_CONF = 1,
        REG_HYST = 2,
        REG_HIGH = 3,
    };

    enum config_bits : u8 {
        CFG_SHUTDOWN = 1u << 0,
        CFG_INT = 1u << 1,
        CFG_POL = 1u << 2,
        CFG_FQUEUE = 3u << 3,
    };

    property<u8> pointer;
    property<u8> config;

    property<u16> temp;
    property<u16> high;
    property<u16> hyst;

    property<u8> i2c_addr;

    i2c_target_socket i2c;
    gpio_initiator_socket alarm;

    lm75(const sc_module_name& nm, u8 addr = 0x48);
    virtual ~lm75() = default;
    VCML_KIND(i2c::lm75);
    virtual void reset();

protected:
    virtual i2c_response i2c_start(const i2c_target_socket& socket,
                                   tlm_command command) override;
    virtual i2c_response i2c_stop(const i2c_target_socket& socket) override;
    virtual i2c_response i2c_read(const i2c_target_socket& socket,
                                  u8& data) override;
    virtual i2c_response i2c_write(const i2c_target_socket& socket,
                                   u8 data) override;
};

} // namespace i2c
} // namespace vcml

#endif
