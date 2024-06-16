/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_H
#define VCML_H

#include "vcml/core/types.h"
#include "vcml/core/version.h"
#include "vcml/core/thctl.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/peq.h"
#include "vcml/core/command.h"
#include "vcml/core/module.h"
#include "vcml/core/component.h"
#include "vcml/core/register.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/processor.h"
#include "vcml/core/system.h"
#include "vcml/core/setup.h"
#include "vcml/core/model.h"

#include "vcml/logging/logger.h"
#include "vcml/logging/inscight.h"

#include "vcml/tracing/tracer.h"
#include "vcml/tracing/tracer_file.h"
#include "vcml/tracing/tracer_term.h"
#include "vcml/tracing/tracer_inscight.h"

#include "vcml/properties/property_base.h"
#include "vcml/properties/property.h"
#include "vcml/properties/broker.h"
#include "vcml/properties/broker_arg.h"
#include "vcml/properties/broker_env.h"
#include "vcml/properties/broker_file.h"
#include "vcml/properties/broker_lua.h"

#include "vcml/debugging/symtab.h"
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
#include "vcml/protocols/usb.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/virtio.h"

#include "vcml/models/generic/clock.h"
#include "vcml/models/generic/reset.h"
#include "vcml/models/generic/bus.h"
#include "vcml/models/generic/memory.h"
#include "vcml/models/generic/gate.h"
#include "vcml/models/generic/gpio.h"
#include "vcml/models/generic/hwrng.h"
#include "vcml/models/generic/fbdev.h"

#include "vcml/models/serial/backend.h"
#include "vcml/models/serial/terminal.h"
#include "vcml/models/serial/uart.h"
#include "vcml/models/serial/pl011.h"
#include "vcml/models/serial/nrf51.h"
#include "vcml/models/serial/cdns.h"
#include "vcml/models/serial/sifive.h"

#include "vcml/models/timers/rtc1742.h"
#include "vcml/models/timers/sp804.h"
#include "vcml/models/timers/nrf51.h"
#include "vcml/models/timers/pl031.h"

#include "vcml/models/block/backend.h"
#include "vcml/models/block/disk.h"

#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/bridge.h"
#include "vcml/models/ethernet/network.h"
#include "vcml/models/ethernet/lan9118.h"
#include "vcml/models/ethernet/ethoc.h"

#include "vcml/models/can/backend.h"
#include "vcml/models/can/bridge.h"
#include "vcml/models/can/bus.h"
#include "vcml/models/can/m_can.h"

#include "vcml/models/i2c/lm75.h"
#include "vcml/models/i2c/oci2c.h"

#include "vcml/models/spi/bus.h"
#include "vcml/models/spi/spi2sd.h"
#include "vcml/models/spi/max31855.h"
#include "vcml/models/spi/flash.h"
#include "vcml/models/spi/ocspi.h"

#include "vcml/models/sd/card.h"
#include "vcml/models/sd/sdhci.h"

#include "vcml/models/usb/xhci.h"
#include "vcml/models/usb/xhcipci.h"
#include "vcml/models/usb/device.h"
#include "vcml/models/usb/keyboard.h"
#include "vcml/models/usb/drive.h"
#include "vcml/models/usb/hostdev.h"

#include "vcml/models/pci/device.h"
#include "vcml/models/pci/host.h"
#include "vcml/models/pci/endpoint.h"

#include "vcml/models/virtio/mmio.h"
#include "vcml/models/virtio/pci.h"
#include "vcml/models/virtio/rng.h"
#include "vcml/models/virtio/blk.h"
#include "vcml/models/virtio/net.h"
#include "vcml/models/virtio/console.h"
#include "vcml/models/virtio/input.h"

#include "vcml/models/meta/loader.h"
#include "vcml/models/meta/simdev.h"
#include "vcml/models/meta/throttle.h"

#include "vcml/models/opencores/ompic.h"
#include "vcml/models/opencores/ockbd.h"
#include "vcml/models/opencores/ocfbc.h"

#include "vcml/models/dma/pl330.h"

#include "vcml/models/arm/pl190vic.h"
#include "vcml/models/arm/syscon.h"
#include "vcml/models/arm/gic400.h"
#include "vcml/models/arm/gicv2m.h"

#include "vcml/models/riscv/clint.h"
#include "vcml/models/riscv/plic.h"
#include "vcml/models/riscv/aclint.h"
#include "vcml/models/riscv/aplic.h"
#include "vcml/models/riscv/simdev.h"

#include "vcml/models/deprecated.h"

#endif
