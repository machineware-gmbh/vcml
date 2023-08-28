


# VCML Models: PL330 (DMA330)

model consists of a manager and up to 8 DMA threads.
up to 32 peripheral interfaces and up to 32 events or interrupts can be configured
The dma executes a custom dma assembly language however this is often abstracted away using a driver.
The model does model instruction faults, however stalling behaviour is only approximated and stalling cannot be varified against this model.
The peripheral interfaces are a work in progress.

----
## Properties
This model has the following properties:

| Property        | Type        | Default   | Description        |
| --------------- | ----------- | --------- | ------------------ |
| `clock`         | `hz_t`      | `3686400` | UART clock (in Hz) |
| `read_latency`  | `sc_time`   | `0ns`     | Extra read delay   |
| `write_latency` | `sc_time`   | `0ns`     | Extra write delay  |
| `backends`      | `string`    | `<empty>` | List of backends   |
| `allow_dmi`     | `bool`      | `true`    | Ignored            |
| `loglvl`        | `log_level` | `info`    | Logging threshold  |
| `trace_errors`  | `bool`      | `false`   | Report TLM errors  |

* `trace: bool`
* `trace_errors: bool`
* `loglvl: log_level`
* `allow_dmi: bool`
* `endian: endianess`
* `read_latency: u32`
* `write_latency: u32`
* `enable_periph: bool`
* `num_channels: u32`
* `num_irq: u32`
* `num_periph: u32`
* `queue_size: u32`
* `mfifo_width: u32`
* `mfifo_lines: u32`
* `clk.trace: bool`
* `clk.trace_errors: bool`
* `rst.trace: bool`
* `rst.trace_errors: bool`
* `channel_0.trace: bool`
* `channel_0.trace_errors: bool`
* `channel_0.loglvl: log_level`
* `channel_1.trace: bool`
* `channel_1.trace_errors: bool`
* `channel_1.loglvl: log_level`
* `channel_2.trace: bool`
* `channel_2.trace_errors: bool`
* `channel_2.loglvl: log_level`
* `channel_3.trace: bool`
* `channel_3.trace_errors: bool`
* `channel_3.loglvl: log_level`
* `channel_4.trace: bool`
* `channel_4.trace_errors: bool`
* `channel_4.loglvl: log_level`
* `channel_5.trace: bool`
* `channel_5.trace_errors: bool`
* `channel_5.loglvl: log_level`
* `channel_6.trace: bool`
* `channel_6.trace_errors: bool`
* `channel_6.loglvl: log_level`
* `channel_7.trace: bool`
* `channel_7.trace_errors: bool`
* `channel_7.loglvl: log_level`
* `manager.trace: bool`
* `manager.trace_errors: bool`
* `manager.loglvl: log_level`
* `in.trace: bool`
* `in.trace_errors: bool`
* `in.allow_dmi: bool`
* `dma.trace: bool`
* `dma.trace_errors: bool`
* `dma.allow_dmi: bool`
* `irq_abort.trace: bool`
* `irq_abort.trace_errors: bool`
* `irq[0].trace: bool`
* `irq[0].trace_errors: bool`
* `irq_abort_stub.trace: bool`
* `irq_abort_stub.trace_errors: bool`

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port         | Type                    | Description                              |
|--------------|-------------------------|------------------------------------------|
| `IN`         | `tlm_target_socket<>`   | Input socket for bus requests            |
| `DMA`        | `tlm_initiator_socket`  | Initiator socket for memory transactions |
| `IRQ`        | `gpio_initiator_array`  | Interrupt port array                     |
| `IRQ_ABORT`  | `gpio_initiator_socket` | Interrupt port                           |
| `PERIPH_IRQ` | `gpio_target_array`     | Interrupt receiver port array            |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name  | Offset | Access | Width | Description                             |
| ----- | ------ | ------ | ----- | --------------------------------------- |
| `THR` | `+0x0` |  R/W   | 8 bit | Rx Buffer (R) / Tx Buffer (W) Register  |
| `IER` | `+0x1` |  R/W   | 8 bit | Interrupt Enable Register               |
| `IIR` | `+0x2` |  R/W   | 8 bit | Interrupt Ident. (R) / FIFO Control (W) |
| `LCR` | `+0x3` |  R/W   | 8 bit | Line Control Register                   |
| `MCR` | `+0x4` |  R/W   | 8 bit | Modem Control Register                  |
| `LSR` | `+0x5` |  R     | 8 bit | Line Status Register                    |
| `MSR` | `+0x6` |  R     | 8 bit | Modem Status Register                   |
| `SCR` | `+0x7` |  R/W   | 8 bit | Scratchpad Register                     |

* `fsrc: u32`
* `inten: u32`
* `int_event_ris: u32`
* `intmis: u32`
* `intclr: u32`
* `dbgstatus: u32`
* `dbgcmd: u32`
* `dbginst0: u32`
* `dbginst1: u32`
* `cr0: u32`
* `cr1: u32`
* `cr2: u32`
* `cr3: u32`
* `cr4: u32`
* `crd: u32`
* `wd: u32`
* `periph_id: u32`
* `pcell_id: u32`
* `channel_X.ftr: u32`
* `channel_X.csr: u32`
* `channel_X.cpc: u32`
* `channel_X.sar: u32`
* `channel_X.dar: u32`
* `channel_X.ccr: u32`
* `channel_X.lc0: u32`
* `channel_X.lc1: u32`
* `manager.dsr: u32`
* `manager.dpc: u32`
* `manager.fsrd: u32`
* `manager.ftrd: u32`

This device model works with Linux:

* bullet point 1
* bullet point2

describe dmatest and point out that driver needs to be included in linux config

`modprobe dmatest timeout=2000 iterations=1 channel=dma0chan0 run=1`

```
pdma0: pdma@12680000 {
        compatible = "arm,pl330", "arm,primecell";
        reg = <0x12680000 0x1000>;
        interrupts = <GIC_SPI 59 IRQ_TYPE_LEVEL_HIGH>;
        #dma-cells = <1>;
        #dma-channels = <8>;
        #dma-requests = <32>;
        clocks = <&clk24mhz>, <&clk24mhz>;
        clock-names = "uartclk", "apb_pclk";
    };
```

----
Documentation August 2023
