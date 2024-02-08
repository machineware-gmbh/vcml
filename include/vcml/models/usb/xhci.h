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
    tlm_target_socket in;
    gpio_initiator_socket irq;

    xhci(const sc_module_name& name);
    virtual ~xhci();
    VCML_KIND(usb::xhci);

    virtual void reset() override;
};

} // namespace usb
} // namespace vcml

#endif
