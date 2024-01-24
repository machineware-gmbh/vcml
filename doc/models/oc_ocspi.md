# VCML Models: OpenCores Tiny SPI Controller
This is a TLM model of the OpenCores Tiny SPI Controller, which serves as a
memory-mapped peripheral component giving the main processor access to the SPI
bus. Note that this controller does not implement the Chip/Slave Select wires,
which must be emulated using GPIOs if you need multiple SPI slaves.

The controller optionally supports interrupts, which will be sent out whenever
a single 8-bit transmission via SPI has started and/or completed. These
interrupts are enabled by writing a `1` to the second or first bit of the
`STATUS` register, respectively. However, since full 8-bit transmission is
modeled using a single transaction, this will generally only result in
interrupt spam. Instead, polling mode (i.e. polling the `STATUS` register)
should be preferred. In this case, you need to stub the interrupt port of the
device, e.g.:

```
vcml::models::ocspi spi_controller("my_spi_controller");
spi_controller.IRQ.stub(); // interrupt not used
```

The SPI bus is modeled using TLM interfaces described in
[`spi.h`](../../include/vcml/spi.h). Fundamentally, SPI slaves must implement
the following interface and have one `vcml::spi_target_socket` bound to itself:

```
class spi_fw_transport_if: public sc_core::sc_interface {
public:
    virtual u8 spi_transport(u8 value_received_from_master) {
        return value_transmitted_to_master;
    }
};
```

More examples for writing your own SPI masters and slaves can be found
[here](../../test/test_spi.cpp).

----
## Properties
This model has the following properties:

| Property        | Type        | Default    | Description               |
| --------------- | ----------- | ---------- | ------------------------- |
| `clock`         | `hz_t`      | `50000000` | SPI clock in Hz (ignored) |
| `read_latency`  | `sc_time`   | `0ns`      | Extra read delay          |
| `write_latency` | `sc_time`   | `0ns`      | Extra write delay         |
| `backends`      | `string`    | `<empty>`  | Ignored                   |
| `loglvl`        | `log_level` | `info`     | Logging threshold         |
| `trace_errors`  | `bool`      | `false`    | Report TLM errors         |

*Note:* the clock property is ignored since SPI transmission is performed
immediatly when the processor writes to the `TXDATA` register.

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).
Note that tracing functionality will cover both TLM and SPI transactions
simultaneously.


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

| Port      | Type                     | Description     |
| --------- | ------------------------ | --------------- |
| `IN`      | `tlm_target_socket<>`    | Slave bus port  |
| `SPI_OUT` | `spi_initiator_socket<>` | Master SPI port |
| `IRQ`     | `sc_out<bool>`           | Interrupt port  |

*Note*: the `IRQ` port must be stubbed in polling mode using `IRQ.stub()`.

----
## Software Interface
The model exposes the following memory mapped registers:

| Name      | Offset  | Access | Width | Description               |
| --------- | ------- | ------ | ----- | ------------------------- |
| `RXDATA`  | `+0x00` |  R     | 32bit | SPI Received Data (LSB)   |
| `TXDATA`  | `+0x04` |  R/W   | 32bit | SPI Transmit Data (LSB)   |
| `STATUS`  | `+0x08` |  R/W   | 32bit | SPI Controller Status     |
| `CONTROL` | `+0x0C` |  R/W   | 32bit | Transmission Mode Control |
| `BAUDDIV` | `+0x10` |  R/W   | 32bit | Baud Rate Divider         |

This device works with Linux, its device driver can be found at:
```
drivers/spi/spi-oc-tiny.c
```

Driver support in the kernel is controlled via `CONFIG_SPI_OC_TINY`. Once this
has been enabled, the SPI controller must receive its own device tree node. The
following example adds an OpenCores Tiny SPI Controller at address `0x96000000`
(and optionally assigns it to interrupt number 7):

```
spi@96000000 {
    #address-cells = <1>;
    #size-cells = <0>;

    compatible = "opencores,tiny-spi-rtlsvn2";
    reg = <0x96000000 0x2000>;
    /* interrupts = <7>; */ /* uncomment to use interrupt mode */
    gpios = <>; /* optionally reference GPIOs here for SPI slave select */
    clock-frequency = <50000000>;
    baud-width = <32>;

    /* here you can add slave devices... */
};
```

----
Documentation updated January 2019
