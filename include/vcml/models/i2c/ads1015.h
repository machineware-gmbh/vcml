/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_ADS1015_H
#define VCML_SPI_ADS1015_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/i2c.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace i2c {

class ads1015 : public module, public i2c_host
{
private:
    enum regs {
        CONVERT = 0,
        CONFIG,
        THRESH_LO,
        THRESH_HI,
        NREGS,
    };

    enum state {
        IDLE,
        STARTED,
        MSB,
        LSB,
    };

    state m_state;

    u8 m_reg_ptr;
    u16 m_regs[NREGS];
    int m_comp_que;

    double read_voltage(int channel);
    void sample_data();
    void sample_thread();
    void update_config();
    void post_read();
    void post_write();

public:
    property<bool> polling;

    property<double> ain0;
    property<double> ain1;
    property<double> ain2;
    property<double> ain3;

    property<u8> i2c_addr;

    i2c_target_socket i2c_in;
    gpio_initiator_socket alert;

    ads1015(const sc_module_name& nm, u8 address);
    virtual ~ads1015();
    VCML_KIND(i2c::ads1015);

protected:
    virtual void before_end_of_elaboration() override;
    virtual void start_of_simulation() override;

    virtual void session_resume() override;

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
