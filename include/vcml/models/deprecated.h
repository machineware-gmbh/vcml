/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_MODEL_DEPRECATED_H
#define VCML_MODEL_DEPRECATED_H

#define VCML_MODEL_DEPRECATED(oldns, oldname, newns, newname) \
    namespace oldns {                                         \
    typedef MWR_DECL_DEPRECATED ::newns::newname oldname;     \
    }

VCML_MODEL_DEPRECATED(vcml::generic, rtc1742, vcml::timers, rtc1742)
VCML_MODEL_DEPRECATED(vcml::generic, lan9118, vcml::ethernet, lan9118)
VCML_MODEL_DEPRECATED(vcml::generic, uart8250, vcml::serial, uart8250)
VCML_MODEL_DEPRECATED(vcml::sd, sdcard, vcml::sd, card)
VCML_MODEL_DEPRECATED(vcml::opencores, ethoc, vcml::ethernet, ethoc)
VCML_MODEL_DEPRECATED(vcml::opencores, ocspi, vcml::spi, ocspi)
VCML_MODEL_DEPRECATED(vcml::opencores, oci2c, vcml::i2c, oci2c)
VCML_MODEL_DEPRECATED(vcml::arm, pl011uart, vcml::serial, pl011)
VCML_MODEL_DEPRECATED(vcml::arm, sp804timer, vcml::timers, sp804)

#endif
