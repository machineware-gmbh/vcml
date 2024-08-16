/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/i2c/sifive.h"

namespace vcml {
namespace i2c {

sifive::sifive(const sc_module_name& nm): oci2c(nm, 2) {
    // sifive-i2c uses opencores-i2c with 4-byte aligned registers
}

sifive::~sifive() {
    // nothing to do
}

void sifive::reset() {
    oci2c::reset();
}

VCML_EXPORT_MODEL(vcml::i2c::sifive, name, args) {
    return new sifive(name);
}

} // namespace i2c
} // namespace vcml
