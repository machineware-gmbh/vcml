/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/xhci.h"

namespace vcml {
namespace usb {

enum xhci_params : u32 {
    XHCI_CAPLENGTH = 0x20,
    XHCI_DBOFF = 0x100,
    XHCI_RTSOFF = 0x200,
};

enum hciversion : u32 {
    HCIVERSION_1_0_0 = 0x100 << 16,
    HCIVERSION_1_1_0 = 0x110 << 16,
    HCIVERSION_RESET = HCIVERSION_1_0_0 | XHCI_CAPLENGTH,
};

xhci::xhci(const sc_module_name& nm):
    peripheral(nm),
    hciversion("hciversion", 0x00, HCIVERSION_RESET),
    hcsparams1("hciparams1", 0x04),
    hcsparams2("hciparams2", 0x08),
    hcsparams3("hciparams3", 0x0c),
    hccparams1("hccparams1", 0x10),
    dboff("dboff", 0x14, XHCI_DBOFF),
    rtsoff("rtsoff", 0x18, XHCI_RTSOFF),
    hccparams2("hccparams2", 0x1c),
    usbcmd("usbcmd", XHCI_CAPLENGTH + 0x00),
    usbsts("usbsts", XHCI_CAPLENGTH + 0x04),
    pagesize("pagesize", XHCI_CAPLENGTH + 0x08),
    dnctrl("dnctrl", XHCI_CAPLENGTH + 0x14),
    crcr("crcr", XHCI_CAPLENGTH + 0x18),
    dcbaap("dcbaap", XHCI_CAPLENGTH + 0x30),
    config("config", XHCI_CAPLENGTH + 0x38),
    in("in"),
    dma("dma"),
    irq("irq") {
    hciversion.sync_never();
    hciversion.allow_read_only();

    hcsparams1.sync_never();
    hcsparams1.allow_read_only();

    hcsparams2.sync_never();
    hcsparams2.allow_read_only();

    hcsparams3.sync_never();
    hcsparams3.allow_read_only();

    hccparams1.sync_never();
    hccparams1.allow_read_only();

    dboff.sync_never();
    dboff.allow_read_only();

    rtsoff.sync_never();
    rtsoff.allow_read_only();

    hccparams2.sync_never();
    hccparams2.allow_read_only();

    usbcmd.sync_always();
    usbcmd.allow_read_write();

    usbsts.sync_always();
    usbsts.allow_read_write();

    pagesize.sync_never();
    pagesize.allow_read_only();

    dnctrl.sync_always();
    dnctrl.allow_read_write();

    crcr.sync_always();
    crcr.allow_read_write();

    dcbaap.sync_always();
    dcbaap.allow_read_write();

    config.sync_always();
    config.allow_read_write();
}

xhci::~xhci() {
    // nothing to do
}

void xhci::reset() {
    peripheral::reset();
}

VCML_EXPORT_MODEL(vcml::usb::xhci, name, args) {
    return new xhci(name);
}

} // namespace usb
} // namespace vcml
