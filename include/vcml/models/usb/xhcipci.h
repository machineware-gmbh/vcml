/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_XHCIPCI_H
#define VCML_USB_XHCIPCI_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"

#include "vcml/models/pci/endpoint.h"
#include "vcml/models/usb/xhci.h"

namespace vcml {
namespace usb {

class xhcipci : public module, public usb_host_if
{
private:
    pci::endpoint m_ep;
    usb::xhci m_xhci;

public:
    pci_base_target_socket pci_in;
    usb_base_initiator_array usb_out;

    xhcipci(const sc_module_name& name);
    virtual ~xhcipci();
    VCML_KIND(usb::xhcipci);
};

} // namespace usb
} // namespace vcml

#endif
