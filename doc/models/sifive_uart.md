# VCML Models: SiFive UART

This is a TLM model of the SiFive UART used for serial I/O. It
models the functional aspects of the real device, so it looks like a real UART
for the software. However, most of the physical aspects have been omitted
(e.g., data is transmitted instantly, transmission errors cannot occur, etc.).

The model uses a tx and rx port to connect to other UARTs. A common choice would
be the terminal uart model, which allows input and output via `stdout` and `stdin`
from a terminal window. Input is polled once per quantum and is only fetched
from the backend if the FIFOs are not full, so data is never dropped.

The model can be operated in interrupt or polling mode.

----
## Properties
This model has the following properties:

| Property        | Type        | Default         | Description             |
|-----------------|-------------|-----------------|-------------------------|
| `clk`           | `hz_t`      | `3686400`       | UART clock (in Hz)      |
| `read_latency`  | `sc_time`   | `0ns`           | Extra read delay        |
| `write_latency` | `sc_time`   | `0ns`           | Extra write delay       |
| `allow_dmi`     | `bool`      | `true`          | Ignored                 |
| `loglvl`        | `log_level` | `info`          | Logging threshold       |
| `trace`         | `bool`      | `false`         | Report TLM transactions |
| `trace_errors`  | `bool`      | `false`         | Report TLM errors       |
| `tx_fifo_size`  | `u32`       | `8`             | Size of the TX FIFO     |
| `rx_fifo_size`  | `u32`       | `8`             | Size of the RX FIFO     |
| `endian`        | `endianess` | `host_endian()` | Endianess               |


Extra properties are available from the specified [`backends`](../backends.md).

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port     | Type                  | Description                   |
|----------| --------------------- |-------------------------------|
| `IN`     | `tlm_target_socket<>` | Input socket for bus requests |
| `TX_IRQ` | `sc_out<bool>`        | TX Watermark Interrupt port   |
| `RX_IRQ` | `sc_out<bool>`        | RX Watermark Interrupt port   |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name     | Offset  | Access | Width  | Description               |
|----------|---------|--------|--------|---------------------------|
| `TXDATA` | `+0x00` | R/W    | 32 bit | Transmit data register    |
| `RXDATA` | `+0x04` | R/W    | 32 bit | Receive data register     |
| `TXCTRL` | `+0x08` | R/W    | 32 bit | Transmit control register |
| `RXCTRL` | `+0x0c` | R/W    | 32 bit | Receive control register  |
| `IE`     | `+0x10` | R/W    | 32 bit | UART interrupt enable     |
| `IP`     | `+0x14` | R      | 32 bit | UART interrupt pending    |
| `DIV`    | `+0x18` | R/W    | 32 bit | Baud rate divisor         |

----
Documentation February 2024
