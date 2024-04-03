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
    0x200,              // bcd_usb
    0,                  // device class
    0,                  // device subclass
    0,                  // device protocol
    8,                  // max packet size
    0xaaaa,             // vendor id
    0x1234,             // device id
    0x0,                // bcd_device
    "MachineWare GmbH", // vendor string
    "VCML Keyboard",    // product string
    VCML_GIT_REV_SHORT, // serial number string
    {
        // configurations
        {
            11,                           // configuration0 value
            USB_CFG_ONE | USB_CFG_WAKEUP, // configuration0 attributes
            40,                           // configuration0 power (80mA)
            {
                // interfaces
                {
                    0,                      // interface0 alternate setting
                    USB_CLASS_MASS_STORAGE, // interface0 class
                    0x06,                   // interface0 subclass (SCSI)
                    0x50,                   // interface0 protocol (Bulk)
                    {
                        // endpoints
                        {
                            usb_ep_in(1), // endpoint1 address
                            USB_EP_BULK,  // endpoint1 attributes
                            512,          // endpoint1 max packet size
                            0,            // endpoint1 intervall (125us units)
                            0,            // endpoint1 refresh
                            0,            // endpoint1 sync address
                        },
                        {
                            usb_ep_out(2), // endpoint2 address
                            USB_EP_BULK,   // endpoint2 attributes
                            512,           // endpoint2 max packet size
                            0,             // endpoint2 intervall (125us units)
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

static const u8 USB3_ENDPOINT_COMPANION_DT[] = {
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

enum msd_command : u8 {
    CMD_FORMAT_UNIT = 0x04,
    CMD_INQUIRY = 0x12,
    CMD_START_STOP = 0x1b,
    CMD_MODE_SELECT = 0x55,
    CMD_MODE_SENSE_6 = 0x1a,
    CMD_MODE_SENSE_10 = 0x5a,
    CMD_ALLOW_MEDIUM_REMOVAL = 0x1e,
    CMD_READ_10 = 0x28,
    CMD_READ_12 = 0xa8,
    CMD_READ_CAPACITY = 0x25,
    CMD_READ_FORMAT_CAPACITY = 0x23,
    CMD_REQUEST_SENSE = 0x03,
    CMD_REZERO_UNIT = 0x01,
    CMD_SEEK_10 = 0x10,
    CMD_SEND_DIAGNOSTIC = 0x1d,
    CMD_TEST_UNIT_READY = 0x00,
    CMD_VERIFY = 0x2f,
    CMD_WRITE_10 = 0x2a,
    CMD_WRITE_12 = 0xaa,
    CMD_WRITE_AND_VERIFY = 0x2e,
    CMD_SYNC_CACHE = 0x35,
};

enum msd_status : u8 {
    STS_SUCCESS = 0,
    STS_ERROR = 1,
    STS_PROTOCOL = 2,
};

enum sense_key : u8 {
    NO_SENSE = 0x00,
    RECOVERED_ERROR = 0x01,
    NOT_READY = 0x02,
    MEDIUM_ERROR = 0x03,
    HARDWARE_ERROR = 0x04,
    ILLEGAL_REQUEST = 0x05,
    UNIT_ATTENTION = 0x06,
    DATA_PROTECT = 0x07,
    BLANK_CHECK = 0x08,
    COPY_ABORTED = 0x0a,
    ABORTED_COMMAND = 0x0b,
    VOLUME_OVERFLOW = 0x0d,
    MISCOMPARE = 0x0e,
};

static const drive::sense SENSE_NOTHING{ 0x00, 0x00, 0x00 };
static const drive::sense SENSE_NO_MEDIUM{ 0x02, 0x3a, 0x00 };

u8 drive::handle_command(u8* cmdbuf) {
    switch (cmdbuf[0]) {
    case CMD_TEST_UNIT_READY: {
        log_debug("test unit ready");
        return STS_SUCCESS;
    }

    case CMD_REQUEST_SENSE: {
        m_output.assign(18, 0);
        m_output[0] = 0xf0;
        m_output[2] = m_sense.key;
        m_output[7] = 10;
        m_output[12] = m_sense.asc;
        m_output[13] = m_sense.ascq;
        log_debug("request sense [0x%02hhx 0x%02hhx 0x%02hhx]", m_sense.key,
                  m_sense.asc, m_sense.ascq);
        return STS_SUCCESS;
    }

    case CMD_INQUIRY: {
        log_debug("inquire request");
        m_output.assign(36, 0);
        m_output[1] = 0x80;
        m_output[3] = 0x01;
        m_output[4] = 36;
        memcpy(&m_output[8], "MWARE  ", 8);
        memcpy(&m_output[16], "vcml flash drive", 16);
        memcpy(&m_output[32], "1.0", 4);
        return STS_SUCCESS;
    }

    case CMD_READ_CAPACITY: {
        if (disk.capacity() == 0) {
            log_debug("read capacity: no medium");
            m_sense = SENSE_NO_MEDIUM;
            return STS_ERROR;
        }

        log_debug("read capacity: %zu bytes", disk.capacity());

        size_t bsz = 512;
        size_t lba = (disk.capacity() / bsz) - 1;

        m_output.assign(8, 0);
        m_output[0] = (u8)(lba >> 24);
        m_output[1] = (u8)(lba >> 16);
        m_output[2] = (u8)(lba >> 8);
        m_output[3] = (u8)(lba >> 0);
        m_output[4] = (u8)(bsz >> 24);
        m_output[5] = (u8)(bsz >> 16);
        m_output[6] = (u8)(bsz >> 8);
        m_output[7] = (u8)(bsz >> 0);
        return STS_SUCCESS;
    }

    case CMD_MODE_SENSE_6: {
        u8 page = cmdbuf[2] & 0x3f;
        log_debug("mode sense (page 0x%02hhx)", page);
        m_output.assign(192, 0);
        u8* p = m_output.data();
        if (disk.readonly)
            p[2] = 0x80;

        p += 4;
        if (page == 8 || page == 0x3f) {
            p[0] = 8;
            p[1] = 0x12;
            p[2] = 0x4;
            p += 20;
        }

        m_output[0] = p - m_output.data() - 1;
        return STS_SUCCESS;
    }

    case CMD_ALLOW_MEDIUM_REMOVAL: {
        if (cmdbuf[4] & 0x1)
            log_debug("preventing media removal");
        else
            log_debug("permitting media removal");
        return STS_SUCCESS;
    }

    case CMD_SYNC_CACHE: {
        log_debug("cache flush requested");
        return STS_SUCCESS;
    }

    case CMD_READ_10:
    case CMD_READ_12: {
        size_t pos = (size_t)cmdbuf[2] << 33 | (size_t)cmdbuf[3] << 25 |
                     (size_t)cmdbuf[4] << 17 | (size_t)cmdbuf[5] << 9;
        log_debug("read offset %zu, %zu bytes", pos, m_len);
        m_output.assign(m_len, 0);
        disk.seek(pos);
        if (!disk.read(m_output.data(), m_output.size()))
            return STS_ERROR;
        return STS_SUCCESS;
    }

    case CMD_WRITE_10:
    case CMD_WRITE_12: {
        size_t pos = (size_t)cmdbuf[2] << 33 | (size_t)cmdbuf[3] << 25 |
                     (size_t)cmdbuf[4] << 17 | (size_t)cmdbuf[5] << 9;
        log_debug("write offset %zu, %zu bytes", pos, m_len);
        disk.seek(pos);
        return STS_SUCCESS;
    }

    default:
        log_error("unsupported command: 0x%02hhx", cmdbuf[0]);
        m_mode = MODE_CSW;
        return STS_PROTOCOL;
    }
}

drive::drive(const sc_module_name& nm, const string& img, bool ro):
    device(nm, DRIVE_DESC),
    m_mode(MODE_CBW),
    m_output(),
    m_len(),
    m_cmd(),
    m_sts(),
    m_tag(),
    m_sense(SENSE_NOTHING),
    usb3("usb3", true),
    vendorid("vendorid", 0xabcd),
    productid("productid", 0x1),
    manufacturer("manufacturer", "MachineWare GmbH"),
    product("product", "VCML Flash Drive"),
    serialno("serialno", VCML_GIT_REV_SHORT),
    image("image", img),
    readonly("readonly", ro),
    disk("disk", image, readonly),
    usb_in("usb_in") {
    m_desc.vendor_id = vendorid;
    m_desc.product_id = productid;
    m_desc.manufacturer = manufacturer;
    m_desc.product = product;
    m_desc.serial_number = serialno;

    if (usb3) {
        m_desc.bcd_usb = 0x300;
        m_desc.max_packet_size0 = 9;
        for (auto& epdesc : m_desc.configs[0].interfaces[0].endpoints) {
            epdesc.max_packet_size = 1024;
            for (auto ch : USB3_ENDPOINT_COMPANION_DT)
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
        csw.signature = 0x53425355;
        csw.tag = m_tag;
        csw.residue = (u32)m_output.size();
        csw.status = m_sts;
        memcpy(data, &csw, sizeof(csw));
        m_mode = MODE_CBW;
        return USB_RESULT_SUCCESS;
    }

    case MODE_DATA_IN: {
        len = min(len, m_output.size());
        memcpy(data, m_output.data(), len);
        m_output.erase(m_output.begin(), m_output.begin() + len);
        if (m_output.empty())
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
        if (cbw.signature != 0x43425355) {
            log_warn("invalid CBW signature 0x%08x", cbw.signature);
            return USB_RESULT_STALL;
        }

        if (cbw.lun > 0) {
            log_warn("invalid LUN: %hhu", cbw.lun);
            return USB_RESULT_STALL;
        }

        if (cbw.data_len == 0)
            m_mode = MODE_CSW;
        else if (cbw.flags & 0x80)
            m_mode = MODE_DATA_IN;
        else
            m_mode = MODE_DATA_OUT;

        m_tag = cbw.tag;
        m_cmd = cbw.cmd[0];
        m_len = cbw.data_len;
        m_sts = handle_command(cbw.cmd);

        return USB_RESULT_SUCCESS;
    }

    case MODE_DATA_OUT: {
        if (m_cmd != CMD_WRITE_10 && m_cmd != CMD_WRITE_12) {
            log_error("invalid data read request");
            return USB_RESULT_STALL;
        }

        len = min(len, m_len);
        if (disk.write(data, len)) {
            m_len -= len;
            if (m_len == 0) {
                m_sts = STS_SUCCESS;
                m_mode = MODE_CSW;
            }
        } else {
            m_sts = STS_ERROR;
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
    m_sense = SENSE_NOTHING;

    m_output.clear();
}

VCML_EXPORT_MODEL(vcml::usb::drive, name, args) {
    if (args.empty())
        return new drive(name);
    else
        return new drive(name, args[0], false);
}

} // namespace usb
} // namespace vcml
