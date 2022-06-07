/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_MODELS_LM75_H
#define VCML_MODELS_LM75_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/gpio.h"
#include "vcml/protocols/i2c.h"

#include "vcml/module.h"

namespace vcml {
namespace generic {

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
        REG_HIGH = 2,
        REG_HYST = 3,
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

    static u16 to_temp9(double temp);
    static double from_temp9(u16 t9);

    lm75(const sc_module_name& nm, u8 addr = 0x48);
    virtual ~lm75() = default;
    VCML_KIND(generic::lm75);
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

} // namespace generic
} // namespace vcml

#endif
