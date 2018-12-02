# VCML Models: OpenCores Framebuffer Controller
This is a TLM model of the OpenCores Framebuffer Controller. 

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description           |
| --------------- | ----------- | ---------- | --------------------- |
| `vncport`       | `u16`       | `0`        | VNC server port       |
| `read_latency`  | `sc_time`   | `0ns`      | Extra read delay      |
| `write_latency` | `sc_time`   | `0ns`      | Extra write delay     |
| `backends`      | `string`    | `<empty>`  | List of backends      |
| `allow_dmi`     | `bool`      | `true`     | Access VRAM using DMI |
| `loglvl`        | `log_level` | `info`     | Logging threshold     |
| `trace_errors`  | `bool`      | `false`    | Report TLM errors     |

Extra properties are available from the specified [`backends`](../backends.md).

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

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

The device driver for Linux can be found at:
```
drivers/video/fbdev/ocfb.c
```

Driver support in the kernel is controlled via `CONFIG_FB_OPENCORES`. Set this
to `y` or `m` to include the driver into the kernel or build it as a module,
respectively.

Once this is done, a new device tree node must be added. The following
example adds an OpenCores Keyboard Controller to address `0x95000000`:

```
framebuffer@94000000 {
    compatible = "opencores,ocfb";
    reg = <0x95000000 0x2000>;
    big-endian;
};
```

----
Documentation Deccember 2018
