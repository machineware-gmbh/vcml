/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_XHCI_H
#define VCML_USB_XHCI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace usb {

class xhci : public peripheral
{
private:
public:
    reg<u32> hciversion;
    reg<u32> hcsparams1;
    reg<u32> hcsparams2;
    reg<u32> hcsparams3;
    reg<u32> hccparams1;
    reg<u32> dboff;
    reg<u32> rtsoff;
    reg<u32> hccparams2;

    reg<u32> usbcmd;
    reg<u32> usbsts;
    reg<u32> pagesize;
    reg<u32> dnctrl;
    reg<u64> crcr;
    reg<u64> dcbaap;
    reg<u32> config;

    tlm_target_socket in;
    tlm_initiator_socket dma;
    gpio_initiator_socket irq;

    xhci(const sc_module_name& name);
    virtual ~xhci();
    VCML_KIND(usb::xhci);

    virtual void reset() override;
};

} // namespace usb
} // namespace vcml

#endif
