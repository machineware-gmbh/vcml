# VCML Models: OpenCores Tiny SPI Controller
This is a TLM model of the OpenCores Tiny SPI Controller

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description           |
| --------------- | ----------- | ---------- | --------------------- |
| `vncport`       | `u16`       | `0`        | VNC server port       |
| `clock`         | `clock_t`   | `60`       | Screen refresh rate   |
| `read_latency`  | `sc_time`   | `0ns`      | Extra read delay      |
| `write_latency` | `sc_time`   | `0ns`      | Extra write delay     |
| `backends`      | `string`    | `<empty>`  | Ignored               |
| `allow_dmi`     | `bool`      | `true`     | Access VRAM using DMI |
| `loglvl`        | `log_level` | `info`     | Logging threshold     |
| `trace_errors`  | `bool`      | `false`    | Report TLM errors     |

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).


----
## Commands
The model supports the following commands during simulation:

| Command       | Description                           |
| ------------- | ------------------------------------- |
| `clist`       | Lists available commands              |
| `cinfo <cmd>` | Shows information about command `cmd` |
| `reset`       | Resets the component                  |
| `abort`       | Aborts the simulation                 |
| `info`        | Prints internal model state           |

In order to execute commands, an active VSP session is required. Tools such
as [`viper`](https://github.com/janweinstock/viper/) can be used as a
graphical frontend for running commands via VSP.

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port  | Type                     | Description     |
| ----- | ------------------------ | --------------- |
| `IN`  | `tlm_target_socket<>`    | Slave bus port  |
| `OUT` | `spi_initiator_socket<>` | Master SPI port |
| `IRQ` | `sc_out<bool>`           | Interrupt port  |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name      | Offset  | Access | Width | Description               |
| --------- | ------- | ------ | ----- | ------------------------- |
| `RXDATA`  | `+0x00` |  R     | 32bit | SPI Received Data (LSB)   |
| `TXDATA`  | `+0x04` |  R/W   | 32bit | SPI Transmit Data (LSB)   |
| `STATUS`  | `+0x08` |  R/W   | 32bit | SPI Controller Status     |
| `CONTROL` | `+0x0C` |  R/W   | 32bit | Transmission Mode Control |
| `BAUD`    | `+0x10` |  R/W   | 32bit | Transmission Baud Rate    |

The device driver for Linux can be found at:
```
drivers/spi/spi-oc-tiny.c
```

----
Documentation updated Deccember 2018
