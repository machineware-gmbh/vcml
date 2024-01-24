# VCML Models: Generic Memory
----
This is a model of a generic memory block that can be used to model RAM and ROM
memories of arbitrary sizes in a virtual platform. The underlying host memory
will be allocated lazily (as needed) and can be aligned arbitrarily (e.g. page
or huge-page aligned for use with KVM). It should be accessed directly via the
TLM Direct Memory Interface (DMI) to facilitate fast simulation.

The model supports the following features:
* `align=n` forces the bottom `n` bits of the underlying memory host address to
  match a given guest address
* `readonly=true`: used when modeling ROM memories; triggers a TLM command
  error upon attempts to write to memory
* `discard_writes=true`: useful when modeling ROM memories that should
  silently ignore write commands, but report no errors
* `images=file_a@0;file_b@0x1000;`: semicolon-separated list of files to load
  after a reset with their corresponding offsets
* `poison=XX`: fills each memory cell with `XX` during reset (but before image
  loading). Useful for detecting memory errors.

----
## Properties
This model has the following properties:

| Property         | Type        | Default    | Description                   |
| ---------------- | ----------- | ---------- | ----------------------------- |
| `size`           | `u64`       | `0`        | Size of memory in bytes       |
| `align`          | `u32`       | `0`        | Alignment bits                |
| `discard_writes` | `bool`      | `false`    | Ignore write commands         |
| `readonly`       | `bool`      | `false`    | Deny write commands (ROM)     |
| `images`         | `string`    | `<empty>`  | List of images to load        |
| `poison`         | `u8`        | `0`        | Memory cell reset value       |
| `read_latency`   | `sc_time`   | `0ns`      | Extra read delay              |
| `write_latency`  | `sc_time`   | `0ns`      | Extra write delay             |
| `backends`       | `string`    | `<empty>`  | Ignored                       |
| `allow_dmi`      | `bool`      | `true`     | Ignored                       |
| `loglvl`         | `log_level` | `info`     | Logging threshold             |
| `trace_errors`   | `bool`      | `false`    | Report TLM errors             |

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

----
## Commands
The model supports the following commands during simulation:

| Command              | Description                                     |
| -------------------- | ----------------------------------------------- |
| `load <file> [off]`  | Load file `file` to memory at offset `off` or 0 |
| `show <start> <end>` | Shows hexdump of memory from `start` to `end`   |
| `clist`              | Lists available commands                        |
| `cinfo <cmd>`        | Shows information about command `cmd`           |
| `reset`              | Resets the component                            |
| `abort`              | Aborts the simulation                           |

In order to execute commands, an active VSP session is required. Tools such
as [`viper`](https://www.machineware.de) can be used as a graphical frontend
for running commands via VSP.

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port  | Type                  | Description                              |
| ----- | --------------------- | ---------------------------------------- |
| `IN`  |`tlm_target_socket<64>`| Input socket for bus data requests       |

----
Documentation updated March 2021
