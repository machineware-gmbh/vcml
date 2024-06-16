/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACER_INSCIGHT_H
#define VCML_TRACER_INSCIGHT_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/tracing/tracer.h"

namespace vcml {

class tracer_inscight : public tracer
{
private:
    template <typename PAYLOAD>
    void do_trace(const activity<PAYLOAD>& msg);

public:
    virtual void trace(const activity<tlm_generic_payload>&) override;
    virtual void trace(const activity<gpio_payload>&) override;
    virtual void trace(const activity<clk_payload>&) override;
    virtual void trace(const activity<pci_payload>&) override;
    virtual void trace(const activity<i2c_payload>&) override;
    virtual void trace(const activity<spi_payload>&) override;
    virtual void trace(const activity<sd_command>&) override;
    virtual void trace(const activity<sd_data>&) override;
    virtual void trace(const activity<vq_message>&) override;
    virtual void trace(const activity<serial_payload>&) override;
    virtual void trace(const activity<eth_frame>&) override;
    virtual void trace(const activity<can_frame>&) override;
    virtual void trace(const activity<usb_packet>&) override;

    tracer_inscight();
    virtual ~tracer_inscight();
};

} // namespace vcml

#endif
