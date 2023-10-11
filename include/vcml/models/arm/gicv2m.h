/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ARM_GICV2M_H
#define VCML_ARM_GICV2M_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/model.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace arm {

class gicv2m : public peripheral
{
private:
    void write_setspi(u32 val);

    u32 read_typer();

    // disabled
    gicv2m();
    gicv2m(const gicv2m&);

public:
    enum gicv2m_consts : size_t {
        PROD_ID = 'M',
        ARCH_VER = 0,
        IMPLEMENTER = 0,
    };

    enum reg_addr : size_t {
        TYPER_ADDR = 0x008,
        SETSPI_ADDR = 0x040,
        IIDR_ADDR = 0xfcc,
    };

    property<size_t> base_spi;
    property<size_t> num_spi;

    typedef field<16, 10> TYPER_BASE_SPI;
    typedef field<0, 10> TYPER_NUM_SPI;
    typedef field<0, 10> SETSPI_SPI;

    reg<u32> typer;  // MSI Type register
    reg<u32> setspi; // Set SPI register
    reg<u32> iidr;   // Interface Identification register

    gpio_initiator_array out;

    tlm_target_socket in;

    gicv2m(const sc_module_name& nm);
    virtual ~gicv2m();
    VCML_KIND(arm::gicv2m);
};

} // namespace arm
} // namespace vcml

#endif
