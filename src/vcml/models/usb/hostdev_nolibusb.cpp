/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/hostdev.h"

namespace vcml {
namespace usb {

void hostdev::init_device() {
    // nothing to do
}

hostdev::hostdev(const sc_module_name& nm, u32 bus, u32 addr):
    device(nm, device_desc()),
    m_device(),
    m_handle(),
    m_ifs(),
    hostbus("hostbus", bus),
    hostaddr("hostaddr", addr),
    usb_in("usb_in") {
    if (hostbus > 0u || hostaddr > 0u)
        log_warn("USB host devices not supported (missing libusb)");

    (void)m_device;
    (void)m_handle;
    (void)m_ifs;
}

usb_result hostdev::set_config(int config) {
    return USB_RESULT_NODEV;
}

hostdev::~hostdev() {
    // nothing to do
}

usb_result hostdev::get_data(u32 ep, u8* data, size_t len) {
    return USB_RESULT_NODEV;
}

usb_result hostdev::set_data(u32 ep, const u8* data, size_t len) {
    return USB_RESULT_NODEV;
}

usb_result hostdev::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                   size_t length) {
    return USB_RESULT_NODEV;
}

void hostdev::usb_reset_device() {
    device::usb_reset_device();
}

VCML_EXPORT_MODEL(vcml::usb::hostdev, name, args) {
    return new hostdev(name);
}

} // namespace usb
} // namespace vcml
