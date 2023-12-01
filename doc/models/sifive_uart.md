# VCML Models: SIFIVE UART
This is a TLM model of the SIFIVE UARTs used for serial I/O. It
models the functional aspects of the real device, so it looks like a real UART
for the software. However, most of the physical aspects have been omitted
(e.g., data is transmitted instantly, transmission errors cannot occur, etc.).

The model uses the backend subsystem for outputting data. A common choice would
be the `term` backend, which allows input and output via `stdout` and `stdin`
from a terminal window. 

----
## Properties
This model has the following properties:

| Property        | Type        | Default   | Description                |
| --------------- | ----------- | --------- | -------------------------- |
| `clock`         | `hz_t`      | `3686400` | UART clock (in Hz)         |
| `read_latency`  | `sc_time`   | `0ns`     | Extra read delay           |
| `write_latency` | `sc_time`   | `0ns`     | Extra write delay          |
| `backends`      | `string`    | `<empty>` | List of backends           |
| `allow_dmi`     | `bool`      | `true`    | Ignored                    |
| `loglvl`        | `log_level` | `info`    | Logging threshold          |
| `trace_errors`  | `bool`      | `false`   | Report TLM errors          |
| `tx_fifo_size`  | `u64`       | `8`       | Default size of tx buffer  |
| `rx_fifo_size`  | `u64`       | `8`       | Default size of rx buffer  |

Extra properties are available from the specified [`backends`](../backends.md).

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port  | Type                  | Description                   |
| ----- | --------------------- | ----------------------------- |
| `IN`  | `tlm_target_socket<>` | Input socket for bus requests |
| `IRQ` | `sc_out<bool>`        | Interrupt port                |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name     | Offset  | Access | Width  | Description                             |
| -------- | ------- | ------ | ------ | --------------------------------------- |
| `txdata` | `+0x00` |  R/W   | 32 bit | Transmit data register                  |
| `rxdata` | `+0x04` |  R     | 32 bit | Receive data register                   |
| `txctrl` | `+0x08` |  R/W   | 32 bit | Transmit control register               |
| `rxctrl` | `+0x0c` |  R/W   | 32 bit | Receive control register                |
| `ie`     | `+0x10` |  R/W   | 32 bit | UART interrupt enable                   |
| `ip`     | `+0x14` |  R     | 32 bit | UART interrupt pending                  |
| `div`    | `+0x18` |  R/W   | 32 bit | Baud rate divisor                       |

This device model works with Linux:

* As a serial device via the device tree: use `compatible = "sifive,uart0";`

The corresponding drivers can be found in `drivers/tty/serial/sifive.c`.

----
Documentation September 2023
