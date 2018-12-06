# VCML Models: OpenCores Keyboard Controller
This is a TLM model of the OpenCores Keyboard Controller. It receives key input
from the host either via the backend subsystem or from a VNC server. To use a
VNC server instead of the backend system, you need to set the property `vncport`
accordingly. Key data is made available to the simulation via a register
interface, which is backed by a 16 character wide FIFO.

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description                   |
| --------------- | ----------- | ---------- | ----------------------------- |
| `fifosize`      | `size_t`    | `16`       | Maximum size of keycode FIFO  |
| `vncport`       | `u16`       | `0`        | VNC server port               |
| `read_latency`  | `sc_time`   | `0ns`      | Extra read delay              |
| `write_latency` | `sc_time`   | `0ns`      | Extra write delay             |
| `backends`      | `string`    | `<empty>`  | Ignored                       |
| `allow_dmi`     | `bool`      | `true`     | Ignored                       |
| `loglvl`        | `log_level` | `info`     | Logging threshold             |
| `trace_errors`  | `bool`      | `false`    | Report TLM errors             |

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

The `vncport` property is only used if VCML has been build with VNC support.

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port  | Type                  | Description                   |
| ----- | --------------------- | ----------------------------- |
| `IN`  | `tlm_target_socket<>` | Input socket for bus requests |
| `IRQ` | `sc_out<bool>`        | Interrupt port                |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name  | Offset | Access | Width | Description       |
| ----- | ------ | ------ | ----- | ----------------- |
| `KHR` | `+0x0` |  R     | 8 bit | Key Hold Register |


The device driver for Linux can be found at:
```
drivers/input/keyboard/opencores-kbd.c
```

Driver support in the kernel is controlled via `CONFIG_KEYBOARD_OPENCORES`. Set
this to `y` or `m` to include the driver into the kernel or build it as a
module, respectively.

However, to get it working with OF device trees, some extra work is required.
The driver does not yet (as of kernel 4.x versions) have a `compatible` match
table, which therefore needs to be added manually to `opencores-kbd.c`:

```
static const struct of_device_id opencores_kbd_of_match[] = {
    { .compatible = "opencores,kbd" },
    { },
};

static struct platform_driver opencores_kbd_device_driver = {
    .probe    = opencores_kbd_probe,
    .driver   = {
        .name = "opencores-kbd",
        .of_match_table = opencores_kbd_of_match,
    },
};
module_platform_driver(opencores_kbd_device_driver);
```

Once this is done, a new device tree node must be added. The following
example adds an OpenCores Keyboard Controller to address `0x93000000` and
assigns it to interrupt `5`:

```
keyboard@93000000 {
    compatible = "opencores,kbd";
    reg = <0x93000000 0x2000>;
    interrupts = <5>;
};
```

----
Documentation updated December 2018
