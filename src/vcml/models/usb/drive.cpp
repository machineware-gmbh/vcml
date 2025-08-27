/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/drive.h"
#include "vcml/core/version.h"

namespace vcml {
namespace usb {

static const device_desc DRIVE_DESC{
    USB_2_0,            // bcd_usb
    0,                  // device class
    0,                  // device subclass
    0,                  // device protocol
    8,                  // max packet size
    0xffff,             // vendor id (patched later)
    0xffff,             // device id (patched later)
    0x0,                // bcd_device
    "MachineWare GmbH", // vendor string
    "VCML USB Drive",   // product string
    VCML_GIT_REV_SHORT, // serial number string
    {
        // configurations
        {
            11,                                  // configuration0 value
            USB_CFG_ONE | USB_CFG_REMOTE_WAKEUP, // configuration0 attributes
            40,                                  // configuration0 power (80mA)
            {
                // interfaces
                {
                    0,                      // interface number
                    0,                      // interface0 alternate setting
                    USB_CLASS_MASS_STORAGE, // interface0 class
                    0x06,                   // interface0 subclass (SCSI)
                    0x50,                   // interface0 protocol (Bulk)
                    0,                      // interface0 name
                    {
                        // endpoints
                        {
                            usb_ep_in(1), // endpoint1 address
                            USB_EP_BULK,  // endpoint1 attributes
                            512,          // endpoint1 max packet size
                            0,            // endpoint1 intervall (125us units)
                            false,        // endpoint1 is_audio
                            0,            // endpoint1 refresh
                            0,            // endpoint1 sync address
                        },
                        {
                            usb_ep_out(2), // endpoint2 address
                            USB_EP_BULK,   // endpoint2 attributes
                            512,           // endpoint2 max packet size
                            0,             // endpoint2 intervall (125us units)
                            false,         // endpoint2 is_audio
                            0,             // endpoint2 refresh
                            0,             // endpoint2 sync address
                        },

                    },
                    {
                        // extra descriptors
                    },
                },
            },
        },
    },
};

static const u8 USB3_ENDPOINT_COMPANION_DT_DRV[] = {
    0x06, // length
    USB_DT_ENDPOINT_COMPANION,
    0x0f, // max burst
    0x00, // attributes
    0x08, // bytes per interval (lo)
    0x00  // bytes per interval (hi)
};

enum usb_bulk_request : u16 {
    USB_REQ_GET_MAX_LUN = 0xfe,
    USB_REQ_RESET_DRIVE = 0xff,
};

#pragma pack(push, 1)
struct usb_msd_cbw {
    u32 signature;
    u32 tag;
    u32 data_len;
    u8 flags;
    u8 lun;
    u8 cmd_len;
    u8 cmd[16];
};

struct usb_msd_csw {
    u32 signature;
    u32 tag;
    u32 residue;
    u8 status;
};

static_assert(sizeof(usb_msd_cbw) == 31, "invalid cbw struct size");
static_assert(sizeof(usb_msd_csw) == 13, "invalid csw struct size");
#pragma pack(pop)

enum msd_status : u8 {
    STS_SUCCESS = 0,
    STS_ERROR = 1,
    STS_PROTOCOL = 2,
};

drive::drive(const sc_module_name& nm, const string& img, bool ro, bool wi):
    device(nm, DRIVE_DESC),
    m_mode(MODE_CBW),
    m_req(),
    m_buflen(),
    m_status(),
    m_tag(),
    usb3("usb3", true),
    vendorid("vendorid", USB_VENDOR_VCML),
    productid("productid", 0x1),
    manufacturer("manufacturer", "MachineWare GmbH"),
    product("product", "VCML Flash Drive"),
    serialno("serialno", VCML_GIT_REV_SHORT),
    image("image", img),
    readonly("readonly", ro),
    writeignore("writeignore", wi),
    disk("disk", image, readonly, writeignore, true),
    usb_in("usb_in") {
    m_desc.vendor_id = vendorid;
    m_desc.product_id = productid;
    m_desc.manufacturer = manufacturer;
    m_desc.product = product;
    m_desc.serial_number = serialno;

    if (usb3) {
        m_desc.bcd_usb = USB_3_0;
        m_desc.max_packet_size0 = 9;
        for (auto& epdesc : m_desc.configs[0].interfaces[0].endpoints) {
            epdesc.max_packet_size = 1024;
            for (auto ch : USB3_ENDPOINT_COMPANION_DT_DRV)
                epdesc.extra.push_back(ch);
        }
    }
}

drive::~drive() {
    // nothing to do
}

usb_result drive::get_data(u32 ep, u8* data, size_t len) {
    if (ep != 1) {
        log_warn("invalid input endpoint: %u", ep);
        return USB_RESULT_STALL;
    }

    switch (m_mode) {
    case MODE_CSW: {
        if (len != sizeof(usb_msd_csw)) {
            log_warn("invalid csw size: %zu", len);
            return USB_RESULT_STALL;
        }

        usb_msd_csw csw;
        csw.signature = fourcc("USBS");
        csw.tag = m_tag;
        csw.residue = 0;
        csw.status = m_status;
        memcpy(data, &csw, sizeof(csw));
        m_mode = MODE_CBW;
        return USB_RESULT_SUCCESS;
    }

    case MODE_DATA_IN: {
        auto& buf = m_req.payload;
        len = min(len, buf.size());
        memcpy(data, buf.data(), len);
        buf.erase(buf.begin(), buf.begin() + len);
        if (buf.empty())
            m_mode = MODE_CSW;
        return USB_RESULT_SUCCESS;
    }

    default:
        log_error("invalid operation mode: %u", m_mode);
        return USB_RESULT_STALL;
    }
}

usb_result drive::set_data(u32 ep, const u8* data, size_t len) {
    if (ep != 2) {
        log_warn("invalid output endpoint: %u", ep);
        return USB_RESULT_STALL;
    }

    switch (m_mode) {
    case MODE_CBW: {
        if (len != sizeof(usb_msd_cbw)) {
            log_warn("invalid cbw size: %zu", len);
            return USB_RESULT_STALL;
        }

        usb_msd_cbw cbw;
        memcpy(&cbw, data, sizeof(usb_msd_cbw));
        if (cbw.signature != fourcc("USBC")) {
            log_warn("invalid CBW signature 0x%08x", cbw.signature);
            return USB_RESULT_STALL;
        }

        if (cbw.lun > 0) {
            log_warn("invalid LUN: %hhu", cbw.lun);
            return USB_RESULT_STALL;
        }

        memcpy(m_req.command, cbw.cmd, 16);
        m_buflen = cbw.data_len;
        m_status = STS_SUCCESS;
        m_tag = cbw.tag;

        if (cbw.flags & 0x80) {
            m_mode = m_buflen ? MODE_DATA_IN : MODE_CSW;
            if (failed(disk.scsi_handle_command(m_req))) {
                m_req.payload.assign(m_buflen, 0xee);
                m_status = STS_ERROR;
            }
        } else {
            m_req.payload.clear();
            m_mode = m_buflen ? MODE_DATA_OUT : MODE_CSW;
        }

        return USB_RESULT_SUCCESS;
    }

    case MODE_DATA_OUT: {
        len = min(len, m_buflen);
        m_req.payload.insert(m_req.payload.end(), data, data + len);
        if (m_req.payload.size() == m_buflen) {
            if (failed(disk.scsi_handle_command(m_req)))
                m_status = STS_SUCCESS;
            m_mode = MODE_CSW;
        }

        return USB_RESULT_SUCCESS;
    }

    default:
        log_error("invalid operation mode: %u", m_mode);
        return USB_RESULT_STALL;
    }
}

usb_result drive::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                 size_t length) {
    switch (req) {
    case USB_REQ_IN | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_GET_MAX_LUN:
        data[0] = 0;
        return USB_RESULT_SUCCESS;
    case USB_REQ_OUT | USB_REQ_CLASS | USB_REQ_IFACE | USB_REQ_RESET_DRIVE:
        log_debug("usb mass storage drive reset requested");
        return USB_RESULT_SUCCESS;
    default:
        return device::handle_control(req, val, idx, data, length);
    }
}

void drive::usb_reset_device() {
    device::usb_reset_device();
    m_mode = MODE_CBW;
    m_req.payload.clear();
}

VCML_EXPORT_MODEL(vcml::usb::drive, name, args) {
    if (args.empty())
        return new drive(name);
    else
        return new drive(name, args[0], false, false);
}

} // namespace usb
} // namespace vcml
