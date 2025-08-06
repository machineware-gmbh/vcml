/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GPIO_PL061_H
#define VCML_GPIO_PL061_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace gpio {

class pl061 : public peripheral
{
private:
    u8 m_status;
    u8 m_prev;

    bool cmd_status(const vector<string>& args, ostream& os);
    bool cmd_set(const vector<string>& args, ostream& os);
    bool cmd_clear(const vector<string>& args, ostream& os);

    void write_dir(u8 val, bool debug);
    void write_is(u8 val, bool debug);
    void write_ibe(u8 val, bool debug);
    void write_iev(u8 val, bool debug);
    void write_ie(u8 val, bool debug);
    void write_ic(u8 val, bool debug);

    void update(bool debug);

public:
    reg<u8> dir;
    reg<u8> is;
    reg<u8> ibe;
    reg<u8> iev;
    reg<u8> ie;
    reg<u8> ris;
    reg<u8> mis;
    reg<u8> ic;
    reg<u8> afsel;

    reg<u32, 4> pid;
    reg<u32, 4> cid;

    gpio_initiator_array<8> gpio_out;
    gpio_target_array<8> gpio_in;
    gpio_initiator_socket intr;
    tlm_target_socket in;

    pl061(const sc_module_name& name);
    virtual ~pl061() = default;
    VCML_KIND(gpio::pl061);

protected:
    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& sbi) override;
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& sbi) override;
    virtual void gpio_notify(const gpio_target_socket& socket) override;
    virtual void end_of_elaboration() override;
};

} // namespace gpio
} // namespace vcml

#endif
