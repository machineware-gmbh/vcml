# VCML Models: FBDEV
This is a TLM model of a generic framebuffer device (FBDEV). It operates on a
fixed memory region, using a fixed resolution and a fixed color depth to output
images using a VNC connection. Conceptually, the generic FBDEV is just a region
of memory that the software/kernel can use to display images, without needing
to know the intricacies of the underlying controller that drives the graphic
processing. This model implements the necessary functionality to fetch the
pixel data from TLM DMI-capable memory and forward to a VNC server.

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description                   |
| --------------- | ----------- | ---------- | ----------------------------- |
| `addr`          | `u64`       | `true`     | Framebuffer memory address    |
| `resx`          | `u32`       | `1280`     | Resolution width in pixels    |
| `resy`          | `u32`       | `720`      | Resolution height in pixels   |
| `vncport`       | `u16`       | `0`        | Port for the VNC server       |
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

| Port       | Type                     | Description            |
| ---------- | ------------------------ | ---------------------- |
| `OUT`      |`tlm_initiator_socket<64>`| Port for memory access |

----
## Software Interface
This model exposes no memory mapped registers.

The driver for using generic framebuffers in the Linux kernel can be found at
```
drivers/video/fbdev/simplefb.c
```
Driver support in the kernel is controlled via `CONFIG_FB` and
`CONFIG_FB_SIMPLE`. Set this to `y` or `m` to include the driver
into the kernel or build it as a module, respectively.

Finally, the kernel must be informed about the memory location, resolution and
color depth of the framebuffer in memory. Add the following to your `chosen`
node in your device tree:

```
chosen {
    bootargs = "...";
    stdout-path = "...";

    #address-cells = <1>;
    #size-cells = <1>;
    ranges;
    fb0: framebuffer@8f800000 {
        compatible = "simple-framebuffer";
        reg = <0x8f800000 (1280*720*4)>;
        width = <1280>;
        height = <720>;
        stride = <(1280*4)>;
        format = "a8r8g8b8";
    };
};
```

This causes the kernel to use 8MiB starting at address `0x8f800000` as a frame-
buffer with a 32bit color depth. Note that you must specify the same resolution
here as in the `fbdev` properties. The color format is always fixed to ARGB
with 8 bits per channel.

If you decide to put the framebuffer into the shared main memory, it is usually
a good idea to exclude the kernel from accidentally mapping it:

```
reserved-memory {
    #address-cells = <1>;
    #size-cells = <1>;
    ranges;

    framebuffer@8f800000 {
        reg = <0x8f800000 0x800000>;
        no-map;
    };
};
```


----
Documentation updated June 2020
