# VCML Tracing

VCML offers transaction tracing functionality for TLM and various other protocols, 
such as GPIO, CLK, PCI, I2C, SPI, SD, Serial, Ethernet, CAN and Virtio.
Tracing can be enabled globally via `system.trace=1` or for individual components via e.g. `system.sdhci.sd_out.trace=1`.

Tracing is available for all known payload types: `tlm_generic_payload`, `gpio_payload`,
`clk_payload`, `pci_payload`, `i2c_payload`, `spi_payload`, `sd_command`, `sd_data`,
`vq_message`, `serial_payload`, `eth_frame`, `can_frame`.

## Tracing Output
VCML currently has two tracing output options:

* `--trace-stdout`: Send tracing output to stdout.
* `-t <file>` or `--trace <file>`: send tracing output to file.

### Tracing Message Format

`[<PROTOCOL> <TIME>] <SOCKET> <DIR> <OP> <ADDRESS> [<VALUE>] (<RESPONSE>)`

Here, `<PROTOCOL>` refers to the specific protocol that was used, `<TIME>` is a transaction timestamp, 
`<SOCKET>` the name of the specific socket involved in the transaction, `<DIR>` refers to the direction of the transaction. 
Outgoing requests are denoted with a `>>`, incoming responses use `<<`. 
`<OP>` refers to the access operation and can be `RD` (read) or `WR` (write). 
`<ADDRESS>` shows the destination address in hex and `<VALUE>` contains the individual byte values of the internal transaction buffer. 
`<RESPONSE>` holds the current state of the transaction, 
see the TLM reference manual for details. 
Following is an example of an actual TLM transaction trace:

* reading four bytes from address 0x100: `[TLM 10.910098127] system.uart0.thr << RD 0x0000100 [00 ff 00 ff] (TLM_OK_RESPONSE)`
* writing single byte to invalid address: `[TLM 10.910098127] system.cpu.hart0.data << WR 0xffffffff [ee] (TLM_ADDRESS_ERROR_RESPONSE)`


----
Documentation January 2024
