/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/device.h"

namespace vcml {
namespace usb {

device::device(const sc_module_name& nm): module(nm), usb_host() {
    // nothing to do
}

device::~device() {
    // nothing to do
}

void device::usb_reset(int ep) {
    if (ep < 0)
        log_info("reset usb device");
    else
        log_info("reset usb endpoint %d", ep);
}

void device::usb_transport(usb_packet& p) {
    log_info("%s", to_string(p).c_str());
}

} // namespace usb
} // namespace vcml
