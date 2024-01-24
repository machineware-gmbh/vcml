# VCML Models: OpenCores Framebuffer Controller
This is a TLM model of the OpenCores Framebuffer Controller. It supports video
output up to 1920x1200 using 32/24/16/8 bit per pixel as well as 8 bit pseudo
color using its iternal palette memory. If VCML has been compiled with VNC
support, you can connect to the model using a VNC client to display the
contents of the video memory.

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description           |
| --------------- | ----------- | ---------- | --------------------- |
| `vncport`       | `u16`       | `0`        | VNC server port       |
| `clock`         | `hz_t`      | `60`       | Screen refresh rate   |
| `read_latency`  | `sc_time`   | `0ns`      | Extra read delay      |
| `write_latency` | `sc_time`   | `0ns`      | Extra write delay     |
| `backends`      | `string`    | `<empty>`  | Ignored               |
| `allow_dmi`     | `bool`      | `true`     | Access VRAM using DMI |
| `loglvl`        | `log_level` | `info`     | Logging threshold     |
| `trace_errors`  | `bool`      | `false`    | Report TLM errors     |

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

The `vncport` property is only used if VCML has been build with VNC support.

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
as [`viper`](https://www.machineware.de) can be used as a graphical frontend
for running commands via VSP.

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port  | Type                     | Description     |
| ----- | ------------------------ | --------------- |
| `IN`  | `tlm_target_socket<>`    | Slave bus port  |
| `OUT` | `tlm_initiator_socket<>` | Master bus port |
| `IRQ` | `sc_out<bool>`           | Interrupt port  |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name    | Offset  | Access | Width | Description                           |
| ------- | ------- | ------ | ----- | ------------------------------------- |
| `CTLR`  | `+0x00` |  R/W   | 32bit | Framebuffer Control Register          |
| `STAT`  | `+0x04` |  R     | 32bit | Framebuffer Status Register           |
| `HTIM`  | `+0x08` |  R/W   | 32bit | Horizontal Timing Register            |
| `VTIM`  | `+0x0C` |  R/W   | 32bit | Vertical Timing Register              |
| `HVLEN` | `+0x10` |  R/W   | 32bit | Horizontal & Vertical Length Register |
| `VBARA` | `+0x14` |  R/W   | 32bit | Video Memory Base Address Register    |

Additionally, there is a small memory for storing palette colors:

| Name    | Start    | End       | Description                                |
| ------- | -------- | --------- | ------------------------------------------ |
| `CLUT0` | `+0x800` | `+0xBFF`  | 256 * 4 bytes for pseudocolor lookup       |
| `CLUT1` | `+0xC00` | `+0xFFF`  | 256 * 4 bytes for pseudocolor lookup (alt) |

The device driver for Linux can be found at:
```
drivers/video/fbdev/ocfb.c
```

Driver support in the kernel is controlled via `CONFIG_FB_OPENCORES`. Set this
to `y` or `m` to include the driver into the kernel or build it as a module,
respectively.

Once this is done, a new device tree node must be added. The following
example adds an OpenCores Framebuffer Controller to address `0x93000000`:

```
framebuffer@93000000 {
    compatible = "opencores,ocfb";
    reg = <0x93000000 0x2000>;
    big-endian;
};
```

Finally, a video mode must be specified on the kernel command line. This can
also be done via the device tree within the `chosen` node:

```
chosen {
    bootargs = "video=ocfb:1024x768-32@60";
};
```

For formatting instructions as well as a list of supported video modes, please
check the file `drivers/video/fbdev/core/modedb.c`.

----
Documentation updated Deccember 2018
