# VCML Models: HWRNG
This is a TLM model of a generic Hardware Random Number Generator (HWRNG). Its
main purpose is to serve as a source for nondeterminism for cryptographic
applications.

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description                   |
| --------------- | ----------- | ---------- | ----------------------------- |
| `pseudo`        | `bool`      | `false`    | Toggle pseudo RNG / HW-RNG    |
| `seed`          | `u32`       | `0`        | Seed used by pseudo RNG       |
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

----
## Software Interface
The model exposes the following memory mapped registers:

| Name | Offset | Access | Width | Description                |
| ---- | -------| ------ | ----- | -------------------------- |
|`RNG` | `0x0`  |  R     | 32bit | Holds 32bit random numbers |

The device driver for Linux can be found at
```
drivers/char/hw_random/timeriomem-rng.c
```

Driver support in the kernel is controlled via `CONFIG_HW_RANDOM` and
`CONFIG_HW_RANDOM_TIMERIOMEM`. Set this to `y` or `m` to include the driver
into the kernel or build it as a module, respectively.

Finally, a new device tree node must be added. Note that the entropy quality
of the HWRNG must be > 0 in order for it to be picked up and used by the
kernel. The following example adds an HWRNG to address 0x99000000:

```
hwrng@99000000 {
    compatible = "timeriomem_rng";
    reg = <0x99000000 0x4>;
    quality = <1000>;
    period = <0>;
};
```

----
Documentation updated March 2020
