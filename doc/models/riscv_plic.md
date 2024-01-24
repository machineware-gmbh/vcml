# VCML Models: RISCV PLIC
This is a TLM model of the RISCV Platform Level Interrupt Controller (PLIC). It
supports multiplexing of up to `NIRQ = 1024` external source interrupts to up
to `NCTX = 15872` target contexts using per-IRQ priorities and per-context
priority thresholds for interrupt arbitration.

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description                   |
| --------------- | ----------- | ---------- | ----------------------------- |
| `read_latency`  | `sc_time`   | `0ns`      | Extra read delay              |
| `write_latency` | `sc_time`   | `0ns`      | Extra write delay             |
| `backends`      | `string`    | `<empty>`  | Ignored                       |
| `allow_dmi`     | `bool`      | `true`     | Ignored                       |
| `loglvl`        | `log_level` | `info`     | Logging threshold             |
| `trace_errors`  | `bool`      | `false`    | Report TLM errors             |

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

In order to execute commands, an active VSP session is required. Tools such
as [`viper`](https://www.machineware.de) can be used as a graphical frontend
for running commands via VSP.

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port  | Type                  | Description                              |
| ----- | --------------------- | ---------------------------------------- |
| `IN`  |`tlm_target_socket<64>`| Input socket for bus requests            |
| `IRQS`|`in_port_list<bool>`   | Input for up to 1024 interrupt sources   |
| `IRQT`|`out_port_list<bool>`  | Output for up to 15872 interrupt targets |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name       | Offset    | Access | Width      | Description                 |
| ---------- | ----------| ------ | ---------- | --------------------------- |
|`PRIORITY`  |`0x0000000`|  R/W   | NIRQ*32bit | Interrupt priority setting  |
|`PENDING`   |`0x0001000`|  R     | NIRQ*1bit  | Interrupt pending status    |
|`ENABLE`    |`0x0002000`|  R/W   | NIRQ*NCTX\*1bit | Per-context IRQ enable |
|`THRESHOLD0`|`0x0200000`|  R/W   | 32bit | Priority threshold for context 0 |
|`CLAIM0`    |`0x0200004`|  R/W   | 32bit | IRQ claim/complete for context 0 |
|`THRESHOLD1`|`0x0201000`|  R/W   | 32bit | Priority threshold for context 1 |
|`CLAIM1`    |`0x0201004`|  R/W   | 32bit | IRQ claim/complete for context 1 |
| ...        | ...       |  ...   | ...   | ...                              |
|`THRESHOLDx`|`0x020x000`|  R/W   | 32bit | Priority threshold for context x |
|`CLAIMx`    |`0x020x004`|  R/W   | 32bit | IRQ claim/complete for context x |
| ...        |`0x3ffffff`|  ...   | ...   | End of PLIC address space (64MB) |

----
Documentation updated September 2019
