# VCML Models: Simulation Throttle
The simulation throttle can be used to limit the simulation speed to a factor
of realtime, configurable via the `rtf` property. If the simulation proceeds
faster than realtime, this throttle can be used to reduce its speed so that it
remains interactive and hardware and software timeouts remain within resonable
limits for human interaction. The `rtf` property can be used to specify the
maximum speed the simulation is allowed to run at. Anything beyond `rtf` will
be throttled; i.e. `rtf = 1.0` limits simulation to realtime (one second of
simulated time per second of realtime), `rtf = 0.5` is half speed (one second
of simulated time per two seconds of realtime) and `rtf = 2.0` means twice as
fast as realtime (two seconds of simulated time per second of realtime). A
value of zero disables throttling entirely.

----
## Properties
This model has the following properties:

| Property          | Type        | Default    | Description             |
| ----------------- | ----------- | ---------- | ----------------------- |
| `loglvl`          | `log_level` | `info`     | Logging threshold       |
| `trace_errors`    | `bool`      | `false`    | Report TLM errors       |
| `update_interval` | `sc_time`   | `10ms`     | Throttle interval       |
| `rtf`             | `double`    | `0.0`      | Target realtime factor  |

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
## Hardware and Software Interface
This model does not expose any ports. It cannot be interacted with from within
the simulation, neither via hardware, nor via software.

----
Documentation updated January 2021
