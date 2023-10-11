
# VCML Models: PL330 (DMA330)

The model comprises a manager along with up to 8 DMA threads.
It allows configuration of up to 32 peripheral interfaces and up to 32 events or interrupts.
The DMA executes a custom DMA assembly language, although this complexity is frequently hidden by a driver abstraction.
While the model does simulate instruction faults, its representation of stalling behavior is only an approximation, and verification against this model cannot confirm stalling. 

`Note: Peripheral interfaces are currently not suported.
The development of peripheral interfaces is currently ongoing.`

----
## Properties
This model has the following properties:

| Property                     | Type        | Default         | Description                             |
|------------------------------|-------------|-----------------|-----------------------------------------|
| `trace`                      | `bool`      | `false`         | Enable Tracing                          |
| `trace_errors`               | `bool`      | `false`         | Enable Error Tracing                    |
| `loglvl`                     | `log_level` | `info`          | Logging threshold                       |
| `allow_dmi`                  | `bool`      | `true`          | `unused(?)`                             |
| `endian`                     | `endianess` | `host_endian()` | `todo description`                      |
| `read_latency`               | `u32`       | `0ns`           | Extra read delay                        |
| `write_latency`              | `u32`       | `0ns`           | Extra write delay                       |
| `enable_periph`              | `bool`      | `false`         | Peripheral Interface enable bit         |
| `num_channels`               | `u32`       | 8               | Number of Channels                      |
| `num_irq`                    | `u32`       | 6               | Maximum Number of Events/IRQs           |
| `num_periph`                 | `u32`       | 6               | Maximum Number of Peripheral Interfaces |
| `queue_size`                 | `u32`       | 16              | Read/Write Queue Size in Instructions   |
| `mfifo_width`                | `u32`       | `32-bit`        | Mfifo Line Data Width                   |
| `mfifo_lines`                | `u32`       | 256             | Mfifo Maximum Number of Lines           |
| `clk.trace`                  | `bool`      | `false`         | Enable Tracing for clk                  |
| `clk.trace_errors`           | `bool`      | `false`         | Enable Error Tracing for clk            |
| `rst.trace`                  | `bool`      | `false`         | Enable Tracing for rst                  |
| `rst.trace_errors`           | `bool`      | `false`         | Enable Error Tracing for rst            |
| `channel_X.trace`            | `bool`      | `false`         | Enable Tracing for channel X            |
| `channel_X.trace_errors`     | `bool`      | `false`         | Enable Error Tracing for Channel X      |
| `channel_X.loglvl`           | `log_level` | `info`          | Logging threshold                       |
| `manager.trace`              | `bool`      | `false`         | Enable Tracing for Manager              |
| `manager.trace_errors`       | `bool`      | `false`         | Enable Error Tracing for Manager        |
| `manager.loglvl`             | `log_level` | `info`          | Logging threshold                       |
| `in.trace`                   | `bool`      | `false`         | Enable Tracing for in                   |
| `in.trace_errors`            | `bool`      | `false`         | Enable Error Tracing for in             |
| `in.allow_dmi`               | `bool`      | `true`          | `todo description`                      |
| `periph_irq[X].trace`        | `bool`      | `false`         | Enable Tracing for periph_irq[X]        |
| `periph_irq[X].trace_errors` | `bool`      | `false`         | Enable Error Tracing for periph_irq[X]  |
| `periph_irq[X].allow_dmi`    | `bool`      | `true`          | `todo description`                      |
| `dma.trace`                  | `bool`      | `false`         | Enable Tracing for dma                  |
| `dma.trace_errors`           | `bool`      | `false`         | Enable Error Tracing for dma            |
| `dma.allow_dmi`              | `bool`      | `true`          | `todo description`                      |
| `irq_abort.trace`            | `bool`      | `false`         | Enable Tracing for irq_abort            |
| `irq_abort.trace_errors`     | `bool`      | `false`         | Enable Error Tracing irq_abort          |
| `irq[X].trace`               | `bool`      | `false`         | Enable Tracing for irq[X]               |
| `irq[X].trace_errors`        | `bool`      | `false`         | Enable Error Tracing for irq[X]         |

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port         | Type                    | Description                              |
|--------------|-------------------------|------------------------------------------|
| `IN`         | `tlm_target_socket`     | Input socket for bus requests            |
| `DMA`        | `tlm_initiator_socket`  | Initiator socket for memory transactions |
| `IRQ`        | `gpio_initiator_array`  | Interrupt port array                     |
| `IRQ_ABORT`  | `gpio_initiator_socket` | Interrupt port                           |
| `PERIPH_IRQ` | `gpio_target_array`     | Interrupt receiver port array            |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name            | Offset              | Access | Width    | Description                         |
|-----------------|---------------------|--------|----------|-------------------------------------|
| `manager.DSR`   | `+0x000`            | R      | 32 bit   | Manager Status Register             |
| `manager.DPC`   | `+0x004`            | R      | 32 bit   | Manager PC Register                 |
| `INTEN`         | `+0x020`            | R/W    | 32 bit   | Interrupt Enable Register           |
| `INT_EVENT_RIS` | `+0x024`            | R      | 32 bit   | Event-Interrupt Raw Status Register |
| `INTMIS`        | `+0x028`            | R      | 32 bit   | Interrupt Status Register           |
| `INTCLR`        | `+0x02c`            | W      | 32 bit   | Interrupt Clear Register            |
| `manager.FSRD`  | `+0x030`            | R      | 32 bit   | Manager Fault Status Register       |
| `FSRC`          | `+0x034`            | R      | 32 bit   | Channel Fault Status Register       |
| `manager.FTRD`  | `+0x038`            | R      | 32 bit   | Manager Fault Type Register         |
| `channel_X.FTR` | `+0x040 + X * 0x04` | R      | 32 bit   | Channel X Fault Type Register       |
| `channel_X.CSR` | `+0x100 + X * 0x08` | R      | 32 bit   | Channel X Status Register           |
| `channel_X.CPC` | `+0x104 + X * 0x08` | R      | 32 bit   | Channel X PC Register               |
| `channel_X.SAR` | `+0x400 + X * 0x20` | R      | 32 bit   | Channel X Source Address            |
| `channel_X.DAR` | `+0x404 + X * 0x20` | R      | 32 bit   | Channel X Destination Address       |
| `channel_X.CCR` | `+0x408 + X * 0x20` | R      | 32 bit   | Channel X Control Register          |
| `channel_X.LC0` | `+0x40c + X * 0x20` | R      | 32 bit   | Channel X Loop Counter 0            |
| `channel_X.LC1` | `+0x410 + X * 0x20` | R      | 32 bit   | Channel X Loop Counter 1            |
| `DBGSTATUS`     | `+0xd00`            | R      | 32 bit   | Debug Status Register               |
| `DBGCMD`        | `+0xd04`            | W      | 32 bit   | Debug Command Register              |
| `DBGINST0`      | `+0xd08`            | W      | 32 bit   | Debug Instruction Register 0        |
| `DBGINST1`      | `+0xd0c`            | W      | 32 bit   | Debug Instruction Register 1        |
| `CR0`           | `+0xe00`            | R      | 32 bit   | Configuration Register 0            |
| `CR1`           | `+0xe04`            | R      | 32 bit   | Configuration Register 1            |
| `CR2`           | `+0xe08`            | R      | 32 bit   | Configuration Register 2            |
| `CR3`           | `+0xe0c`            | R      | 32 bit   | Configuration Register 3            |
| `CR4`           | `+0xe10`            | R      | 32 bit   | Configuration Register 4            |
| `CRD`           | `+0xe14`            | R      | 32 bit   | Manager Configuration Register      |
| `WD`            | `+0xe80`            | R/W    | 32 bit   | Watchdog Register                   |
| `PERIPH_ID`     | `+0xfe0`            | R      | 4x32 bit | Peripheral Identification Register  |
| `PCELL_ID`      | `+0xff0`            | R      | 4x32 bit | Component Identification Register   |

This device model works with Linux:

The DMATest module can be used to confirm the correct functioning of DMA channels,
ensuring reliable and efficient DMA data transfer.
To run DMATest it needs to be included in the linux configuration.

Example of usage:
`modprobe dmatest timeout=2000 iterations=1 channel=dma0chan0 run=1`


The pl330 dma model can be included in the device tree as follows:
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
