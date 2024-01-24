# VCML Models: Image Loader
The image loader is a simulation-only, infrastructure device used for loading
ELF binaries to memory. Image files can be specified as a semicolon separated
list of filenames in the `images` property or by executing the `load_elf`
module command. The loader will use its `INSN` and `DATA` TLM ports to load
exectuable and data sections from the ELF, respectively. Automatic image
loading is performed during device reset, which is always performed at
simulation startup and whenever the `RESET` signal is asserted.

*Note*: this will likely race with the reset handler of your memory, which can
potentially overwrite or clear image data written to memory by the loader.
Suggested workaround is to instantiate the loader after your memory.

----
## Properties
This model has the following properties:

| Property          | Type        | Default    | Description             |
| ----------------- | ----------- | ---------- | ----------------------- |
| `loglvl`          | `log_level` | `info`     | Logging threshold       |
| `trace_errors`    | `bool`      | `false`    | Report TLM errors       |
| `images`          | `string`    | `empty`    | List of files to load   |

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

----
## Commands
The model supports the following commands during simulation:

| Command           | Description                               |
| ----------------- | ----------------------------------------- |
| `clist`           | Lists available commands                  |
| `cinfo <cmd>`     | Shows information about command `cmd`     |
| `reset`           | Resets the component                      |
| `abort`           | Aborts the simulation                     |
| `load_elf <file>` | Reads ELF `<file>` and loads it to memory |

In order to execute commands, an active VSP session is required. Tools such
as [`viper`](https://www.machineware.de) can be used as a graphical frontend
for running commands via VSP.

----
## Hardware and Software Interface
The following ports and sockets must be connected prior to simulating:

| Port    | Type                  | Description                              |
| ------- | --------------------- | ---------------------------------------- |
| `INSN`  |`tlm_target_socket<64>`| Used for loading executable segments     |
| `DATA`  |`tlm_target_socket<64>`| Used for loading non-executable segments |

----
Documentation updated February 2021
