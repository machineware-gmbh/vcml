# VCML Models: SD Host Controller Interface
This is a TLM model of the Fujitsu F_SDH30 SD Host Controller Interface (SDHCI),
which serves as a memory-mapped peripheral component giving the main processor
the possibility to communicate to an MMC/SD card.

The host driver can write the command argument to the argument register ARG.
Afterwards the command should be written to the command register CMD. For the
command index bits 13-08 are reserved and for some execution flags bits 07-00.
After the command register has been written, the controller forwards the
argument and the command to the SD card bound to the SD_OUT initiator socket.
The responsonse of the SD card is stored in the RESPONSE register and an
interrupt is triggered. The kind of the interrupt (e.g. command complete,
transaction complete) is shown in the normal interrupt status register. The
status of the controller is shown in the present state register all the time.
To write data to the sd card CMD24 (single block) or CMD25 (multiple block) has
to be send. If an interrupt is triggered by the controller, the the host can
start transferring data to the controller via the BUFFER_DATA_PORT register.
Reading commands are CMD17 (single block) and CMD18 (multiple block). The
controller provides the data to the host in the BUFFER_DATA_PORT register.

For more information see [SD Host Controller Simplified Specification](https://www.sdcard.org/downloads/pls/index.html).

----
## Hardware Interface
The following ports and sockets must be connected prior to simulating:

| Port     | Type                    | Description     |
| -------- | ----------------------- | --------------- |
| `IN`     | `tlm_target_socket<>`   | Slave bus port  |
| `SD_OUT` | `sd_initiator_socket<>` | Master SD port  |
| `IRQ`    | `sc_out<bool>`          | Interrupt port  |

----
## Software Interface
The model exposes the following memory mapped registers:

| Name                      | Offset   | Access    | Width   | Description                         |
| ------------------------- | -------- | --------- | ------- | ----------------------------------- |
| `BLOCK_SIZE`              | `+0x004` |  RW       | 16bit   | Block Size Register                 |
| `BLOCK_COUNT_16BIT`       | `+0x006` |  RW       | 16bit   | 16-bit Block Count Register         |
| `ARG`                     | `+0x008` |  RW       | 32bit   | Argument Register                   |
| `TRANSFER_MODE`           | `+0x00c` |  RW       | 16bit   | Transfer Mode Register              |
| `CMD`                     | `+0x00e` |  RW       | 16bit   | Command Register                    |
| `RESPONSE`                | `+0x010` |  ROC      | 4*32bit | Response Register                   |
| `BUFFER_DATA_PORT`        | `+0x020` |  RW       | 32bit   | Buffer Data Port Register           |
| `PRESENT_STATE`           | `+0x024` |  RO/ROC   | 32bit   | Present State Register              |
| `HOST_CONTROL_1`          | `+0x028` |  RW       | 8bit    | Host Control 1 Register             |
| `POWER_CTRL`              | `+0x029` |  RW       | 8bit    | Power Control Register              |
| `CLOCK_CTRL`              | `+0x02c` |  RW/ROC   | 16bit   | Clock Control Register              |
| `TIMEOUT_CTRL`            | `+0x02e` |  RW       | 8bit    | Timeout Control Register            |
| `SOFTWARE_RESET`          | `+0x02f` |  RWAC     | 8bit    | Software Reset Register             |
|                           |          |           |         |                                     |
| `NORMAL_INT_STAT`         | `+0x030` |  ROC/RW1C | 16bit   | Normal Interrupt Status Register    |
| `ERROR_INT_STAT`          | `+0x032` |  RW1C     | 16bit   | Error Interrupt Status Register     |
| `NORMAL_INT_STAT_ENABLE`  | `+0x034` |  RW       | 16bit   | Normal Interrupt Status Enable Reg. |
| `ERROR_INT_STAT_ENABLE`   | `+0x036` |  RW       | 16bit   | Error Interrupt Status Enable Reg.  |
| `NORMAL_INT_SIG_ENABLE`   | `+0x038` |  RW       | 16bit   | Normal Interrupt Signal Enable Reg. |
| `ERROR_INT_SIG_ENABLE`    | `+0x03a` |  RW       | 16bit   | Error Interrupt Signal Enable Reg.  |
|                           |          |           |         |                                     |
| `CAPABILITIES`            | `+0x040` |  HWInit   | 2*32bit | Capabilities Register               |
| `MAX_CURR_CAP`            | `+0x048` |  HWInit   | 32bit   | Max. Current Capabilities Register  |
| `HOST_CONTROLLER_VERSION` | `+0x0fe` |  HWInit   | 16bit   | Host Controller Version Register    |
| `F_SDH30_AHB_CONFIG`      | `+0x100` |  RW       | 16bit   | Controller specific Register        |
| `F_SDH30_ESD_CONTROL`     | `+0x124` |  RW       | 32bit   | Controller specific Register        |

Note:
* `ROC`: read-only, cleared on reset
* `RW1C`: read-only, write one to clear
* `RWAC`: read/write, automatic clear
* `HWInit`: read-only, initialized by the device

The device driver for Linux can be found at:
```
drivers/mmc/host/sdhci_f_sdh30.c
```

Driver support in the kernel is controlled via `CONFIG_MMC_SDHCI_F_SDH30`. Set
this to `y` or `m` to include the driver into the kernel or build it as a
module, respectively.

Once this is done, a new device tree node must be added. The following example
adds an Fujitsu F_SDH30 SD Host Controller Interface to address `0x99000000`:

```
sdhciclk: clk50mhz {
    compatible = "fixed-clock";
    #clock-cells = <0>;
    clock-frequency = <50000000>;
};

sdhci0: mmc@99000000 {
    compatible = "fujitsu,mb86s70-sdhci-3.0";
    reg = <0x99000000 0x1000>;
    interrupts = <8>
    clocks = <&sdhciclk>, <&sdhciclk>;
    clock-names = "iface", "core";
};
```
----
Documentation updated March 2020
