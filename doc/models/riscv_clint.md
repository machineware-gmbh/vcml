# VCML Models: RISCV CLINT
This is a TLM model of the RISCV Core Local Interruptor (CLINT). It supports
interrupt generation from software, typically used for inter-processor-inter-
facing, as well as timer interrupts. Each CLINT can drive up to 4095 harts.

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

| Port       | Type                  | Description                            |
| ---------- | --------------------- | -------------------------------------- |
| `IN`       |`tlm_target_socket<64>`| Input socket for bus requests          |
| `IRQ_SW`   |`out_port_list<bool>`  | Output for up to 4095 SW interrupts    |
| `IRQ_TIMER`|`out_port_list<bool>`  | Output for up to 4095 Timer interrupts |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name          | Offset    | Access | Width   | Description                  |
| ------------- | ----------| ------ | ------- | ---------------------------- |
|`MSIP_0`       |`0x0000000`|  R/W   | 32bit   | Hart 0 SW-IRQ generator      |
|`MSIP_1`       |`0x0000004`|  R/W   | 32bit   | Hart 1 SW-IRQ generator      |
| ...           | ...       |  ...   | ...     | ...                          |
|`MSIP_4094`    |`0x0003ff8`|  R/W   | 32bit   | Hart 4094 SW-IRQ generator   |
|`MTIMECMP_0`   |`0x0004000`|  R/W   | 64bit   | Hart 0 timer compare value   |
|`MTIMECMP_1`   |`0x0004008`|  R/W   | 64bit   | Hart 1 timer compare value   |
| ...           | ...       |  ...   | ...     | ...                          |
|`MTIMECMP_4094`|`0x000bff0`|  R/W   | 64bit   | Hart 4094 time compare value |
|`MTIME`        |`0x000bff8`|  R     | 64bit   | Current time counter         |

----
Documentation updated January 2020
