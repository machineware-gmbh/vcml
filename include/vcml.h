/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#ifndef VCML_H
#define VCML_H

#include "vcml/core/types.h"
#include "vcml/core/strings.h"
#include "vcml/core/utils.h"
#include "vcml/core/report.h"
#include "vcml/core/version.h"
#include "vcml/core/thctl.h"
#include "vcml/core/bitops.h"
#include "vcml/core/socket.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/aio.h"
#include "vcml/core/library.h"
#include "vcml/core/command.h"
#include "vcml/core/module.h"
#include "vcml/core/component.h"
#include "vcml/core/register.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/processor.h"
#include "vcml/core/system.h"
#include "vcml/core/setup.h"

#include "vcml/logging/logger.h"
#include "vcml/logging/publisher.h"
#include "vcml/logging/log_file.h"
#include "vcml/logging/log_stream.h"
#include "vcml/logging/log_term.h"

#include "vcml/tracing/tracer.h"
#include "vcml/tracing/tracer_file.h"
#include "vcml/tracing/tracer_term.h"

#include "vcml/properties/property_base.h"
#include "vcml/properties/property.h"
#include "vcml/properties/broker.h"
#include "vcml/properties/broker_arg.h"
#include "vcml/properties/broker_env.h"
#include "vcml/properties/broker_file.h"

#include "vcml/debugging/symtab.h"
#include "vcml/debugging/elf_reader.h"
#include "vcml/debugging/srec_reader.h"
#include "vcml/debugging/target.h"
#include "vcml/debugging/loader.h"
#include "vcml/debugging/subscriber.h"
#include "vcml/debugging/suspender.h"
#include "vcml/debugging/rspserver.h"
#include "vcml/debugging/gdbarch.h"
#include "vcml/debugging/gdbserver.h"
#include "vcml/debugging/vspserver.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/video.h"
#include "vcml/ui/display.h"
#include "vcml/ui/console.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/clk.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/sd.h"
#include "vcml/protocols/i2c.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/pci_ids.h"
#include "vcml/protocols/eth.h"
#include "vcml/protocols/can.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/virtio.h"

#include "vcml/models/generic/clock.h"
#include "vcml/models/generic/reset.h"
#include "vcml/models/generic/bus.h"
#include "vcml/models/generic/memory.h"
#include "vcml/models/generic/gpio.h"
#include "vcml/models/generic/uart8250.h"
#include "vcml/models/generic/lan9118.h"
#include "vcml/models/generic/rtc1742.h"
#include "vcml/models/generic/hwrng.h"
#include "vcml/models/generic/fbdev.h"

#include "vcml/models/serial/backend.h"
#include "vcml/models/serial/terminal.h"

#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/bridge.h"
#include "vcml/models/ethernet/network.h"

#include "vcml/models/can/backend.h"
#include "vcml/models/can/bridge.h"
#include "vcml/models/can/bus.h"

#include "vcml/models/i2c/lm75.h"

#include "vcml/models/spi/bus.h"
#include "vcml/models/spi/spi2sd.h"
#include "vcml/models/spi/max31855.h"

#include "vcml/models/sd/sdcard.h"
#include "vcml/models/sd/sdhci.h"

#include "vcml/models/pci/device.h"
#include "vcml/models/pci/host.h"

#include "vcml/models/virtio/mmio.h"
#include "vcml/models/virtio/pci.h"
#include "vcml/models/virtio/rng.h"
#include "vcml/models/virtio/console.h"
#include "vcml/models/virtio/input.h"

#include "vcml/models/meta/loader.h"
#include "vcml/models/meta/simdev.h"
#include "vcml/models/meta/throttle.h"

#include "vcml/models/opencores/ompic.h"
#include "vcml/models/opencores/ethoc.h"
#include "vcml/models/opencores/ockbd.h"
#include "vcml/models/opencores/ocfbc.h"
#include "vcml/models/opencores/ocspi.h"
#include "vcml/models/opencores/oci2c.h"

#include "vcml/models/arm/pl011uart.h"
#include "vcml/models/arm/pl190vic.h"
#include "vcml/models/arm/sp804timer.h"
#include "vcml/models/arm/syscon.h"
#include "vcml/models/arm/gic400.h"

#include "vcml/models/riscv/clint.h"
#include "vcml/models/riscv/plic.h"
#include "vcml/models/riscv/simdev.h"

#endif
