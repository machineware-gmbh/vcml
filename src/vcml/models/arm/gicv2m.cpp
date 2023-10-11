/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/arm/gicv2m.h"

namespace vcml {
namespace arm {

void gicv2m::write_setspi(u32 val) {
    val &= SETSPI_SPI::MASK;
    if (val >= base_spi && val < base_spi + num_spi)
        out[val].pulse();
}

u32 gicv2m::read_typer() {
    return TYPER_BASE_SPI::set(base_spi) | TYPER_NUM_SPI::set(num_spi);
}

gicv2m::gicv2m(const sc_module_name& nm):
    peripheral(nm),
    base_spi("base_spi", 64),
    num_spi("num_spi", 64),
    typer("typer", TYPER_ADDR),
    setspi("setspi", SETSPI_ADDR),
    iidr("iidr", IIDR_ADDR, PROD_ID << 20 | ARCH_VER << 16 | IMPLEMENTER),
    out("out"),
    in("in") {
    if (base_spi > TYPER_BASE_SPI())
        VCML_ERROR("base_spi property out of range: %zu", base_spi.get());

    if (num_spi > TYPER_NUM_SPI())
        VCML_ERROR("num_spi property out of range: %zu", num_spi.get());

    typer.allow_read_only();
    typer.on_read(&gicv2m::read_typer);

    setspi.allow_write_only();
    setspi.on_write(&gicv2m::write_setspi);

    iidr.allow_read_only();
}

gicv2m::~gicv2m() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::arm::gicv2m, name, args) {
    return new gicv2m(name);
}

} // namespace arm
} // namespace vcml
