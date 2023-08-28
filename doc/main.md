# VCML Documentation
The Virtual Components Modeling Library (`vcml`) is an addon library for SystemC
that provides facilities to ease construction of Virtual Platforms (VPs).
Roughly speaking, its contributions can be separated into two areas:
*Modeling Primitives* and *Hardware Models*. Modeling Primitives refer to
utilities such as improved TLM sockets, port lists and registers and are 
intended to serve as building blocks for new models.
Hardware Models, such as UARTs, Timers, Memories etc. are also available
based on actual hardware models from various vendors or common implementations.
These models generally work with their corresponding Linux device drivers and
can be used as *off-the-shelf* components to swiftly assemble an entire VP.

* Documentation files for VCML modeling primitives:
  * [Logging](logging.md)
  * [Components](components.md)
  * [Properties](properties.md)
  * [Backends](backends.md)
  * [Peripherals](peripherals.md)
  * [Session](session.md)

* Documentation for VCML hardware models:
  * [Generic Memory](models/generic_mem.md)
  * [Generic Bus](models/generic_bus.md)
  * [8250 UART](models/uart8250.md)
  * [DS1742 RTC](models/rtc1742.md)
  * [SDHCI](models/sdhci.md)
  * [HWRNG](models/hwrng.md)
  * [FBDEV](models/fbdev.md)
  * [VIRTIO Models](models/virtio.md)
  * [OpenCores ETHOC](models/oc_ethoc.md)
  * [OpenCores OMPIC](models/oc_ompic.md)
  * [OpenCores OCKBD](models/oc_ockbd.md)
  * [OpenCores OCFBC](models/oc_ocfbc.md)
  * [OpenCores OCSPI](models/oc_ocspi.md)
  * [ARM PL011 UART](models/arm_pl011.md)
  * [ARM PL190 VIC](models/arm_pl190.md)
  * [ARM SP804 Dual Timer](models/arm_sp804.md)
  * [ARM GIC400](models/arm_gic400.md)
  * [ARM PL330 DMA](models/dma_pl330.md)
  * [RISC-V CLINT](models/riscv_clint.md)
  * [RISC-V PLIC](models/riscv_plic.md)
  * [Simulation Loader](models/meta_loader.md)
  * [Simulation Device](models/meta_simdev.md)
  * [Simulation Throttle](models/meta_throttle.md)

----
Documentation January 2021
