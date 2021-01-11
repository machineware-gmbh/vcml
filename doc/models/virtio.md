# VCML Models: VIRTIO
VIRTIO is a standard for virtualized hardware. It can be used whenever you need
a certain function of a hardware IP component (persistant storage, entropy,
accelerated graphics, networking, etc.) but do not care about the actual
hardware implementation and software driver being used. Conceptually, all
VIRTIO devices can be split into a controller and a device part. The
*controller* handles all bus communication with the guest processor (via MMIO
or PCIe), while the *device* implements the actual functionality (ethernet,
entropy, input, graphics, etc.). More information about VIRTIO can be found
at [[1]](https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html).

----
## VIRTIO MMIO
This model serves as a VIRTIO controller that is accesses via memory-mapped IO
registers (MMIO). Connection to a VIRTIO device is established via its
`VIRTIO_OUT` TLM port. This model supports both *split* and *packed* virtqueues
for transmission of data to and from its connected VIRTIO device, controllable
via its `use_packed_queues` property.

### Properties
The `virtio::mmio` model has the following properties:

| Property              | Type        | Default    | Description             |
| --------------------- | ----------- | ---------- | ----------------------- |
| `use_packed_queues`   | `bool`      | `false`    | Virtqueue type selector |
| `use_strong_barriers` | `bool`      | `false`    | Request strong barriers |
| `read_latency`        | `sc_time`   | `0ns`      | Extra read delay        |
| `write_latency`       | `sc_time`   | `0ns`      | Extra write delay       |
| `backends`            | `string`    | `<empty>`  | Ignored                 |
| `allow_dmi`           | `bool`      | `true`     | Ignored                 |
| `loglvl`              | `log_level` | `info`     | Logging threshold       |
| `trace_errors`        | `bool`      | `false`    | Report TLM errors       |

The properties `loglvl` and `trace_errors` require [`loggers`](../logging.md).

### Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port          | Typ e                       | Description                    |
| ------------- | --------------------------- | ------------------------------ |
| `IN`          | `tlm_target_socket<>`       | Input socket for bus requests  |
| `OUT`         | `tlm_initiator_socket<>`    | Output socket for bus requests |
| `VIRTIO_OUT`  | `virtio_initiator_socket<>` | VIRTIO connection to device    |
| `IRQ`         | `sc_out<bool>`              | Interrupt output port          |

### Software Interface
The `virtio::mmio` exposes the following memory mapped registers:

| Name                  | Offset  | Access | Width | Description                   |
| --------------------- | ------- | ------ | ----- | ----------------------------- |
| `MAGIC`               | `+0x00` |  R     | 32bit | Magic value `0x74726976`      |
| `VERSION`             | `+0x04` |  R     | 32bit | VIRTIO version                |
| `DEVICE_ID`           | `+0x08` |  R     | 32bit | VIRTIO device ID              |
| `VENDOR_ID`           | `+0x0c` |  R     | 32bit | VIRTIO device vendor          |
| `DEVICE_FEATURES`     | `+0x10` |  R     | 32bit | Device features               |
| `DEVICE_FEATURES_SEL` | `+0x14` |  W     | 32bit | Device feature select         |
| `DRIVER_FEATURES`     | `+0x20` |  W     | 32bit | Driver features               |
| `DRIVER_FEATURES_SEL` | `+0x24` |  W     | 32bit | Driver feature select         |
| `QUEUE_SEL`           | `+0x30` |  W     | 32bit | Virtqueue selector            |
| `QUEUE_NUM_MAX`       | `+0x34` |  R     | 32bit | Max virtqueue size            |
| `QUEUE_NUM`           | `+0x38` |  W     | 32bit | Virtqueue size                |
| `QUEUE_READY`         | `+0x44` |  R/W   | 32bit | Virtqueue initialization      |
| `QUEUE_NOTIFY`        | `+0x50` |  W     | 32bit | Virtqueue data notify         |
| `INTERRUPT_STATUS`    | `+0x60` |  R     | 32bit | Interrupt status              |
| `INTERRUPT_ACK`       | `+0x64` |  W     | 32bit | Interrupt acknowledge         |
| `STATUS`              | `+0x70` |  R/W   | 32bit | Controller & device status    |
| `QUEUE_DESC_LO`       | `+0x80` |  W     | 32bit | Descriptor address low 32bit  |
| `QUEUE_DESC_HI`       | `+0x84` |  W     | 32bit | Descriptor address high 32bit |
| `QUEUE_DRIVER_LO`     | `+0x90` |  W     | 32bit | Driver address low 32bit      |
| `QUEUE_DRIVER_HI`     | `+0x94` |  W     | 32bit | Driver address high 32bit     |
| `QUEUE_DEVICE_LO`     | `+0xa0` |  W     | 32bit | Device address low 32bit      |
| `QUEUE_DEVICE_HI`     | `+0xa4` |  W     | 32bit | Device address high 32bit     |
| `CONFIG_GEN`          | `+0xfc` |  R     | 32bit | Current config generation     |

Additionally, there is a device specific configuration memory between
`0x100..0xfff`.

The device driver for Linux can be found at
```
drivers/virtio/virtio_mmio.c
```

In order to allow the kernel to discover this `virtio::mmio` device, a new
device-tree node needs to be added. The following example adds a VIRTIO
controller to address `0x14000000` using interrupt `12`. Afterwards, the
connected VIRTIO device will be discovered automatically.

```
virtio@14000000 {
    compatible = "virtio,mmio";
    reg = <0x14000000 0x1000>;
    interrupts = <12>;
};
```

### Commands
This model supports the following commands during simulation:

| Command       | Description                           |
| ------------- | ------------------------------------- |
| `clist`       | Lists available commands              |
| `cinfo <cmd>` | Shows information about command `cmd` |
| `reset`       | Resets the component                  |
| `abort`       | Aborts the simulation                 |

----
## VIRTIO Devices
Below is a list of currently available VIRTIO devices and the function they
implement. They need to be connected to a VIRTIO controller (e.g.
`virtio::mmio`) via their `VIRTIO_IN` port.

| Device                  | ID     | Description                          |
| ----------------------- | ------ | -------------------------------------|
| `vcml::virtio::rng`     | `0x04` | Used to supply entropy to the system |
| `vcml::virtio::input`   | `0x12` | Keyboard and touchpad event device   |
| `vcml::virtio::console` | `0x03` | Serial hypervisor console device     |

----
Documentation updated January 2021
