# VCML Models: Simulation Device
The simulation device (simdev) can be used to allow software running within a
simulation to control and interact with the simulator. Use-cases revolve mostly
around unit testing (e.g. to exit or abort the simulation and return an error
code under certain circumstances) and benchmarking simulation performance (e.g.
to read out the current host time and use it for performance calculation in
dhrystone or coremark). The simulation device also has registers that allow
sending data to the standard output of the simulator, which can be used for
printing debug info during early kernel- or firmware boot.

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

| Port  | Type                    | Description                   |
| ----- | ----------------------- | ----------------------------- |
| `IN`  | `tlm_target_socket<64>` | Input socket for bus requests |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name   | Offset  | Access | Width  | Description                           |
| ------ | ------- | ------ | ------ | ------------------------------------- |
| `STOP` | `+0x00` |   W    | 32 bit | Stop sim on write using `sc_stop()`   |
| `EXIT` | `+0x08` |   W    | 32 bit | Exit sim on write using `exit()`      |
| `ABRT` | `+0x10` |   W    | 32 bit | Abort sim on write using `abort()`    |
| `SCLK` | `+0x18` |   R    | 64 bit | Returns current simulation time       |
| `HCLK` | `+0x20` |   R    | 64 bit | Returns host time in microseconds     |
| `SOUT` | `+0x28` |   W    | 32 bit | Writes the value to `stdout`          |
| `SERR` | `+0x30` |   W    | 32 bit | Writes the value to `stderr`          |
| `PRNG` | `+0x38` |   R    | 32 bit | Returns a random value using `rand()` |

----
Documentation updated May 2019
