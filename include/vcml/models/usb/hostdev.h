/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_HOSTDEV_H
#define VCML_USB_HOSTDEV_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/usb.h"
#include "vcml/models/usb/device.h"

struct libusb_context;
struct libusb_device;
struct libusb_device_handle;

namespace vcml {
namespace usb {

class hostdev : public device
{
private:
    struct libusb_device* m_device;
    struct libusb_device_handle* m_handle;

    struct hostdev_interface {
        bool detached;
        bool claimed;
    } m_ifs[16];

    void init_device();
    usb_result set_config(int i);

public:
    property<u32> hostbus;
    property<u32> hostaddr;

    usb_target_socket usb_in;

    hostdev(const sc_module_name& nm): hostdev(nm, 0, 0) {}
    hostdev(const sc_module_name& nm, u32 bus, u32 addr);
    virtual ~hostdev();
    VCML_KIND(usb::hostdev);

protected:
    virtual usb_result get_data(u32 ep, u8* data, size_t len) override;
    virtual usb_result set_data(u32 ep, const u8* data, size_t len) override;

    virtual usb_result handle_control(u16 req, u16 val, u16 idx, u8* data,
                                      size_t length) override;

    virtual void usb_reset_device() override;
};

} // namespace usb
} // namespace vcml

#endif
