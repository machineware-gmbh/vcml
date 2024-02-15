/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_DEVICE_H
#define VCML_USB_DEVICE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/usb.h"

namespace vcml {
namespace usb {

class device : public module, public usb_dev_if
{
public:
    device(const sc_module_name& nm);
    virtual ~device();
    VCML_KIND(usb::device);

protected:
    virtual void usb_reset_device() override;
    virtual void usb_reset_endpoint(int ep) override;
    virtual void usb_transport(usb_packet& p) override;
};

} // namespace usb
} // namespace vcml

#endif
