/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_DRIVE_H
#define VCML_USB_DRIVE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/usb.h"
#include "vcml/models/usb/device.h"
#include "vcml/models/block/scsi.h"

namespace vcml {
namespace usb {

class drive : public device
{
private:
    enum drive_mode {
        MODE_CBW,
        MODE_DATA_OUT,
        MODE_DATA_IN,
        MODE_CSW,
    };

    drive_mode m_mode;
    block::scsi_request m_req;
    size_t m_buflen;
    u32 m_status;
    u32 m_tag;

    u8 handle_command(u8* cmdbuf);

public:
    property<bool> usb3;

    property<u16> vendorid;
    property<u16> productid;

    property<string> manufacturer;
    property<string> product;
    property<string> serialno;

    property<string> image;
    property<bool> readonly;
    property<bool> writeignore;

    block::scsi_disk disk;

    usb_target_socket usb_in;

    drive(const sc_module_name& nm):
        drive(nm, "ramdisk:512MiB", false, false) {}
    drive(const sc_module_name& nm, const string& image, bool readonly,
          bool writeignore);
    virtual ~drive();
    VCML_KIND(usb::drive);

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
