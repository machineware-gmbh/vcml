/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/block/scsi.h"
#include "vcml/core/version.h"

namespace vcml {
namespace block {

const char* scsi_command_str(u8 opcode) {
    switch (opcode) {
    case SCSI_TEST_UNIT_READY:
        return "TEST_UNIT_READY";
    case SCSI_REZERO_UNIT:
        return "REZERO_UNIT";
    case SCSI_REQUEST_SENSE:
        return "REQUEST_SENSE";
    case SCSI_FORMAT_UNIT:
        return "FORMAT_UNIT";
    case SCSI_SEEK_10:
        return "SEEK_10";
    case SCSI_INQUIRY:
        return "INQUIRY";
    case SCSI_MODE_SENSE_6:
        return "MODE_SENSE_6";
    case SCSI_START_STOP:
        return "START_STOP";
    case SCSI_SEND_DIAGNOSTIC:
        return "SEND_DIAGNOSTIC";
    case SCSI_ALLOW_MEDIUM_REMOVAL:
        return "ALLOW_MEDIUM_REMOVAL";
    case SCSI_READ_FORMAT_CAPACITY:
        return "READ_FORMAT_CAPACITY";
    case SCSI_READ_CAPACITY:
        return "READ_CAPACITY";
    case SCSI_READ_10:
        return "READ_10";
    case SCSI_WRITE_10:
        return "WRITE_10";
    case SCSI_WRITE_AND_VERIFY:
        return "WRITE_AND_VERIFY";
    case SCSI_VERIFY:
        return "VERIFY";
    case SCSI_SYNC_CACHE:
        return "SYNC_CACHE";
    case SCSI_WRITE_SAME_10:
        return "WRITE_SAME_10";
    case SCSI_MODE_SELECT:
        return "MODE_SELECT";
    case SCSI_MODE_SENSE_10:
        return "MODE_SENSE_10";
    case SCSI_WRITE_SAME_16:
        return "WRITE_SAME_16";
    case SCSI_READ_CAPACITY_16:
        return "SCSI_READ_CAPACITY_16";
    case SCSI_REPORT_LUNS:
        return "SCSI_REPORT_LUNS";
    case SCSI_READ_12:
        return "READ_12";
    case SCSI_WRITE_12:
        return "WRITE_12";
    default:
        return "UNKNOWN";
    }
}

bool scsi_command_transfers_to_device(u8 opcode) {
    switch (opcode) {
    case SCSI_WRITE_10:
    case SCSI_WRITE_12:
    case SCSI_MODE_SELECT:
    case SCSI_FORMAT_UNIT:
        return true;
    default:
        return false;
    }
}

bool scsi_command_transfers_from_device(u8 opcode) {
    return !scsi_command_transfers_to_device(opcode);
}

const char* scsi_response_str(u8 opcode) {
    switch (opcode) {
    case SCSI_GOOD:
        return "GOOD";
    case SCSI_CHECK_CONDITION:
        return "CHECK_CONDITION";
    case SCSI_CONDITION_MET:
        return "CONDITION_MET";
    case SCSI_BUSY:
        return "BUSY";
    case SCSI_INTERMEDIATE:
        return "INTERMEDIATE";
    case SCSI_INTERMEDIATE_COND_MET:
        return "INTERMEDIATE_COND_MET";
    case SCSI_RESERVATION_CONFLICT:
        return "RESERVATION_CONFLICT";
    case SCSI_COMMAND_TERMINATED:
        return "COMMAND_TERMINATED";
    case SCSI_TASK_SET_FULL:
        return "TASK_SET_FULL";
    case SCSI_ACA_ACTIVE:
        return "ACA_ACTIVE";
    case SCSI_TASK_ABORTED:
        return "TASK_ABORTED";
    default:
        return "INVALID";
    }
}

const scsi_sense SENSE_NOTHING{ SCSI_NO_SENSE, 0x00, 0x00 };
const scsi_sense SENSE_NOT_READY{ SCSI_NOT_READY, 0x04, 0x00 };
const scsi_sense SENSE_NO_MEDIUM{ SCSI_NOT_READY, 0x3a, 0x00 };
const scsi_sense SENSE_MEDIUM_ERROR{ SCSI_MEDIUM_ERROR, 0x11, 0x00 };
const scsi_sense SENSE_ILLEGAL_REQ{ SCSI_ILLEGAL_REQUEST, 0x20, 0x00 };
const scsi_sense SENSE_ILLEGAL_FIELD{ SCSI_ILLEGAL_REQUEST, 0x24, 0x00 };
const scsi_sense SENSE_ILLEGAL_PARAM{ SCSI_ILLEGAL_REQUEST, 0x26, 0x00 };
const scsi_sense SENSE_UNIT_ATTENTION{ SCSI_UNIT_ATTENTION, 0x28, 0x00 };
const scsi_sense SENSE_DATA_PROTECT{ SCSI_DATA_PROTECT, 0x27, 0x00 };

u64 scsi_read(const u8* ptr, size_t len) {
    VCML_ERROR_ON(len > 8, "attempt to read more than 8 bytes");

    u64 result = 0;
    for (size_t i = 0; i < len; i++) {
#ifdef MWR_HOST_LITTLE_ENDIAN
        result |= (u64)ptr[i] << ((len - i - 1) * 8);
#else
        result |= (u64)ptr[i] << (i * 8);
#endif
    }

    return result;
}

void scsi_write(u8* ptr, size_t len, u64 data) {
    VCML_ERROR_ON(len > 8, "attempt to write more than 8 bytes");
    for (size_t i = 0; i < len; i++) {
#ifdef MWR_HOST_LITTLE_ENDIAN
        ptr[i] = data >> ((len - i - 1) * 8);
#else
        ptr[i] = data >> (i * 8);
#endif
    }
}

void scsi_write_str(u8* buf, size_t len, const string& str) {
    for (size_t i = 0; i < len - 1; i++)
        buf[i] = i < str.length() ? str[i] : ' ';
    if (len > 0)
        buf[len - 1] = 0;
}

void scsi_write_sense(u8* buf, size_t len, const scsi_sense& sense) {
    VCML_ERROR_ON(len < 18, "sense buffer too small");
    buf[0] = 0xf0;
    buf[2] = sense.key;
    buf[7] = 0x0a;
    buf[12] = sense.asc;
    buf[13] = sense.ascq;
}

void scsi_write_block_limits(u8* p, size_t len, const scsi_block_limits& bl) {
    VCML_ERROR_ON(len < 60, "block limit buffer too small");
    scsi_write(p + 4, 1, bl.wnr ? 1 : 0);
    scsi_write(p + 5, 1, bl.max_compare_and_write_length);
    scsi_write(p + 6, 2, bl.opt_transfer_length_gran);
    scsi_write(p + 8, 4, bl.max_transfer_length);
    scsi_write(p + 12, 4, min(bl.opt_transfer_length, bl.max_transfer_length));
    scsi_write(p + 16, 4, bl.max_prefetch_length);
    scsi_write(p + 20, 4, bl.max_unmap_lba_count);
    scsi_write(p + 24, 4, bl.max_unmap_blk_desc_count);
    scsi_write(p + 28, 4, bl.opt_unmap_gran);
    scsi_write(p + 32, 4, bl.unmap_gran_align);
    scsi_write(p + 36, 8, bl.max_write_same_len);
    scsi_write(p + 44, 4, bl.max_atomic_transfer_length);
    scsi_write(p + 48, 4, bl.atomic_align);
    scsi_write(p + 52, 4, bl.atomic_transfer_length_gran);
}

scsi_response scsi_disk::scsi_inquiry(scsi_request& req) {
    if (req.command[1] & 1)
        return scsi_inquiry_vpd(req);

    log_debug("inquire request");
    req.payload.assign(36, 0);
    req.payload[0] = SCSI_DEVICE_DIRECT_ACCESS;
    req.payload[1] = removable ? 0x80 : 0x00;
    req.payload[2] = 0x06;        // SPC-4
    req.payload[3] = 0x10 | 0x02; // HiSup, Format2
    req.payload[4] = req.payload.size() - 5;
    scsi_write_str(req.payload.data() + 8, 8, vendor);
    scsi_write_str(req.payload.data() + 16, 16, product);
    scsi_write_str(req.payload.data() + 32, 4, revision);
    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_inquiry_vpd(scsi_request& req) {
    u8 page = req.command[2];
    log_debug("inquire vital product data 0x%02hhx", page);

    req.payload.push_back(SCSI_DEVICE_DIRECT_ACCESS);
    req.payload.push_back(page);
    req.payload.push_back(0x00);
    req.payload.push_back(0x00);

    switch (page) {
    case 0x00: { // list of supported pages
        req.payload.push_back(0x00);
        req.payload.push_back(0x80);
        req.payload.push_back(0x83);
        req.payload.push_back(0xb0);
        req.payload.push_back(0xb1);
        req.payload.push_back(0xb2);
        break;
    }

    case 0x80: { // device serial number
        size_t len = min<size_t>(32, serial.get().length());
        req.payload.insert(req.payload.end(), len, ' ');
        scsi_write_str(&req.payload.back() - len, len, serial);
        break;
    }

    case 0x83: { // device identification page
        if (!product.get().empty()) {
            size_t len = min<size_t>(product.get().length(), 255 - 8);
            req.payload.push_back(0x02); // ASCII
            req.payload.push_back(0x00); // unofficial
            req.payload.push_back(0x00); // reserved
            req.payload.push_back((u8)len);
            req.payload.insert(req.payload.end(), len, ' ');
            scsi_write_str(&req.payload.back() - len, len, product);
        }

        if (device_wwn) {
            req.payload.push_back(0x01);
            req.payload.push_back(0x03);
            req.payload.push_back(0x00);
            req.payload.push_back(0x08);
            req.payload.insert(req.payload.end(), 8, 0);
            scsi_write(&req.payload.back() - 8, 8, device_wwn);
        }

        if (port_wwn) {
            req.payload.push_back(0x61);
            req.payload.push_back(0x93);
            req.payload.push_back(0x00);
            req.payload.push_back(0x08);
            req.payload.insert(req.payload.end(), 8, 0);
            scsi_write(&req.payload.back() - 8, 8, port_wwn);
        }

        if (port_idx) {
            req.payload.push_back(0x61);
            req.payload.push_back(0x94);
            req.payload.push_back(0x00);
            req.payload.push_back(0x04);
            req.payload.insert(req.payload.end(), 4, 0);
            scsi_write(&req.payload.back() - 4, 4, port_idx);
        }
        break;
    }

    case 0xb0: { // block limits
        req.payload.insert(req.payload.end(), 60, 0);
        scsi_block_limits bl{};
        bl.max_transfer_length = 4096 / blocksize();
        bl.opt_transfer_length = 4096 / blocksize();
        bl.max_unmap_blk_desc_count = 255;
        bl.max_write_same_len = 1;
        scsi_write_block_limits(req.payload.data(), req.payload.size(), bl);
        break;
    }

    case 0xb1: { // block device characteristics
        req.payload.insert(req.payload.end(), 36, 0);
        break;
    }

    case 0xb2: { // thin provisioning data
        req.payload.push_back(0x00);
        req.payload.push_back(0xe0); // write_same10/16
        req.payload.push_back(0x02);
        req.payload.push_back(0x00);
        break;
    }

    default:
        log_warn("invalid product data page: 0x%02hhx", page);
        m_sense = SENSE_ILLEGAL_REQ;
        return SCSI_CHECK_CONDITION;
    }

    size_t len = req.payload.size() - 4;
    req.payload[2] = len >> 8;
    req.payload[3] = len & 0xff;
    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_report_luns(scsi_request& req) {
    log_debug("reporting single lun present");
    req.payload.assign(16, 0);
    req.payload[3] = 8; // single lun
    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_request_sense(scsi_request& req) {
    req.payload.assign(18, 0);
    scsi_write_sense(req.payload.data(), req.payload.size(), m_sense);
    log_debug("request sense [0x%02hhx 0x%02hhx 0x%02hhx]", m_sense.key,
              m_sense.asc, m_sense.ascq);
    m_sense = SENSE_NOTHING;
    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_mode_sense(scsi_request& req, bool is10) {
    u8 size = req.command[0] == SCSI_MODE_SENSE_10 ? 2 : 1;
    u8 page = req.command[2] & 0x3f;
    u8 ctrl = req.command[2] & 0xc0;
    bool dbd = req.command[1] & 0x08;

    if (page == SCSI_MODE_PAGE_ALL_PAGES)
        log_debug("mode sense (all pages)");
    else
        log_debug("mode sense (page 0x%02hhx)", page);

    req.payload.assign(size, 0);                   // total size placeholder
    req.payload.push_back(0x00);                   // default media type
    req.payload.push_back(readonly ? 0x80 : 0x00); // media specifc data
    req.payload.insert(req.payload.end(), size > 1 ? 4 : 1, 0); // bdsize

    if (!dbd) { // include block descriptors
        size_t blksz = blocksize();
        size_t nblks = mwr::udivup(capacity(), blksz);
        req.payload.back() = 8; // record bdsize
        req.payload.insert(req.payload.end(), 8, 0);
        scsi_write(&req.payload.back() - 7, 3, nblks);
        scsi_write(&req.payload.back() - 3, 3, blksz);
    }

    if (page == SCSI_MODE_PAGE_ALL_PAGES) {
        for (u8 pg = 0; pg < SCSI_MODE_PAGE_ALL_PAGES; pg++)
            scsi_write_mode_page(req.payload, pg, ctrl, true);
    } else {
        scsi_write_mode_page(req.payload, page, ctrl, false);
    }

    // patch in final size at offset 0
    scsi_write(req.payload.data(), size, req.payload.size() - size);
    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_read_capacity(scsi_request& req, bool is16) {
    if (capacity() == 0) {
        log_debug("read capacity: no medium");
        m_sense = SENSE_NO_MEDIUM;
        return SCSI_CHECK_CONDITION;
    }

    log_debug("read capacity (%s): %zu bytes", is16 ? "16" : "10", capacity());

    size_t bsz = blocksize();
    size_t lba = udivup(capacity(), bsz) - 1;

    if (is16) {
        req.payload.assign(32, 0);
        scsi_write(req.payload.data() + 0, 8, lba);
        scsi_write(req.payload.data() + 8, 4, bsz);
    } else {
        req.payload.assign(8, 0);
        scsi_write(req.payload.data() + 0, 4, min<u64>(lba, U32_MAX));
        scsi_write(req.payload.data() + 4, 4, bsz);
    }

    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_handle_seek(scsi_request& req) {
    size_t off = scsi_read(req.command + 2, 4) * blocksize();
    log_debug("seek offset %zu", off);

    if (off > capacity()) {
        log_warn("seek offset out of bounds: %zu", off);
        m_sense = SENSE_ILLEGAL_FIELD;
        return SCSI_CHECK_CONDITION;
    }

    if (seek(off)) {
        m_sense = SENSE_MEDIUM_ERROR;
        return SCSI_CHECK_CONDITION;
    }

    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_handle_read(scsi_request& req, bool is12) {
    size_t pos = is12 ? 6 : 7;
    size_t num = is12 ? 4 : 2;
    size_t off = scsi_read(req.command + 2, 4) << blockbits;
    size_t len = scsi_read(req.command + pos, num) << blockbits;
    log_debug("read offset %zu, %zu bytes", off, len);

    if (len > capacity() || off > capacity() - len) {
        log_warn("read out of bounds: %zu/%zu", off, len);
        m_sense = SENSE_ILLEGAL_FIELD;
        return SCSI_CHECK_CONDITION;
    }

    req.payload.assign(len, 0);

    if (!seek(off) || !read(req.payload.data(), req.payload.size())) {
        m_sense = SENSE_MEDIUM_ERROR;
        return SCSI_CHECK_CONDITION;
    }

    return SCSI_GOOD;
}

scsi_response scsi_disk::scsi_handle_write(scsi_request& req, bool is12) {
    size_t pos = is12 ? 6 : 7;
    size_t num = is12 ? 4 : 2;
    size_t off = scsi_read(req.command + 2, 4) << blockbits;
    size_t len = scsi_read(req.command + pos, num) << blockbits;
    log_debug("write offset %zu, %zu bytes", off, len);

    if (len > capacity() || off > capacity() - len) {
        log_warn("write out of bounds: %zu/%zu", off, len);
        m_sense = SENSE_ILLEGAL_FIELD;
        return SCSI_CHECK_CONDITION;
    }

    if (readonly) {
        m_sense = SENSE_DATA_PROTECT;
        return SCSI_CHECK_CONDITION;
    }

    if (writeignore)
        return SCSI_GOOD;

    if (seek(off) && write(req.payload.data(), req.payload.size()))
        return SCSI_GOOD;

    m_sense = SENSE_MEDIUM_ERROR;
    return SCSI_CHECK_CONDITION;
}

static bool is_zero(const vector<u8>& data) {
    for (u8 byte : data) {
        if (byte)
            return false;
    }

    return true;
}

scsi_response scsi_disk::scsi_write_same(scsi_request& req, bool is16) {
    size_t pos = is16 ? 10 : 7;
    size_t num = is16 ? 4 : 2;
    size_t off = scsi_read(req.command + 2, is16 ? 8 : 4) << blockbits;
    size_t len = scsi_read(req.command + pos, num) << blockbits;
    log_debug("write_same offset %zu, %zu bytes", off, len);

    if (len > capacity() || off > capacity() - len) {
        log_warn("write_same out of bounds: %zu/%zu", off, len);
        m_sense = SENSE_ILLEGAL_FIELD;
        return SCSI_CHECK_CONDITION;
    }

    if (readonly) {
        m_sense = SENSE_DATA_PROTECT;
        return SCSI_CHECK_CONDITION;
    }

    if (writeignore)
        return SCSI_GOOD;

    if (req.payload.empty() || is_zero(req.payload)) {
        bool may_unmap = req.command[1] & bit(3);
        if (seek(off) && wzero(len, may_unmap))
            return SCSI_GOOD;

        m_sense = SENSE_MEDIUM_ERROR;
        return SCSI_CHECK_CONDITION;
    }

    size_t end = off + len;
    while (off < end) {
        if (!seek(off)) {
            m_sense = SENSE_MEDIUM_ERROR;
            return SCSI_CHECK_CONDITION;
        }

        size_t n = min(req.payload.size(), end - off);
        if (!write(req.payload.data(), n)) {
            m_sense = SENSE_MEDIUM_ERROR;
            return SCSI_CHECK_CONDITION;
        }

        off += n;
    }

    return SCSI_GOOD;
}

scsi_disk::scsi_disk(const sc_module_name& nm, const string& image,
                     bool readonly, bool writeignore, bool remov):
    disk(nm, image, readonly, writeignore),
    m_sense(SENSE_NOTHING),
    removable("removable", remov),
    blockbits("blockbits", 9),
    device_wwn("device_wwn", 0),
    port_wwn("port_wwn", 0),
    port_idx("port_idx", 0),
    vendor("vendor", "MWARE"),
    product("product", "VCML-SCSIDRIVE"),
    revision("revision", "1.0") {
    // nothing to do
}

scsi_disk::~scsi_disk() {
    // nothing to do
}

scsi_response scsi_disk::scsi_handle_command(scsi_request& req) {
    log_debug("received command %s", scsi_command_str(req.command[0]));
    switch (req.command[0]) {
    case SCSI_TEST_UNIT_READY: {
        log_debug("test unit ready");
        return SCSI_GOOD;
    }

    case SCSI_REQUEST_SENSE:
        return scsi_request_sense(req);

    case SCSI_FORMAT_UNIT: {
        log_debug("format unit");
        return SCSI_GOOD;
    }

    case SCSI_INQUIRY:
        return scsi_inquiry(req);

    case SCSI_REPORT_LUNS:
        return scsi_report_luns(req);

    case SCSI_ALLOW_MEDIUM_REMOVAL: {
        if (req.command[4] & 0x1)
            log_debug("preventing media removal");
        else
            log_debug("permitting media removal");
        return SCSI_GOOD;
    }

    case SCSI_SYNC_CACHE: {
        log_debug("cache flush requested");
        flush();
        return SCSI_GOOD;
    }

    case SCSI_READ_CAPACITY:
        return scsi_read_capacity(req, false);
    case SCSI_READ_CAPACITY_16:
        return scsi_read_capacity(req, true);

    case SCSI_SEEK_10:
        return scsi_handle_seek(req);

    case SCSI_READ_10:
        return scsi_handle_read(req, false);
    case SCSI_READ_12:
        return scsi_handle_read(req, true);

    case SCSI_WRITE_10:
        return scsi_handle_write(req, false);
    case SCSI_WRITE_12:
        return scsi_handle_write(req, true);

    case SCSI_WRITE_SAME_10:
        return scsi_write_same(req, false);
    case SCSI_WRITE_SAME_16:
        return scsi_write_same(req, true);

    case SCSI_MODE_SENSE_6:
        return scsi_mode_sense(req, false);
    case SCSI_MODE_SENSE_10:
        return scsi_mode_sense(req, true);

    default:
        log_warn("illegal request: 0x%02hhx", req.command[0]);
        m_sense = SENSE_ILLEGAL_REQ;
        return SCSI_CHECK_CONDITION;
    }
}

void scsi_disk::scsi_write_mode_page(vector<u8>& data, u8 page, u8 control,
                                     bool all) {
    switch (page) {
    case SCSI_MODE_PAGE_RW_ERROR: {
        data.push_back(0x01); // header: page no
        data.push_back(0x0a); // header: page size
        data.push_back(0x80); // flags: awre
        data.insert(data.end(), 9, 0x00);
        return;
    }

    case SCSI_MODE_PAGE_CACHING: {
        data.push_back(0x08); // header: page no
        data.push_back(0x12); // header: page size
        data.push_back(0x04); // cache control (read cache disable)
        data.insert(data.end(), 17, 0x00);
        return;
    }

    case SCSI_MODE_PAGE_CONTROL: {
        data.push_back(0x0a); // header: page no
        data.push_back(0x06); // header: page size
        data.push_back(0x00); // reserved
        data.push_back(0x01); // d_sense
        data.insert(data.end(), 4, 0x00);
        return;
    }

    case SCSI_MODE_PAGE_EXCEPTIONS: {
        data.push_back(0x1c); // header: page no
        data.push_back(0x0a); // header: page size
        data.push_back(0x10); // dexcept
        data.push_back(0x03); // mrie
        data.insert(data.end(), 8, 0x00);
        return;
    }

    case SCSI_MODE_PAGE_CAPABILITIES: {
        u16 r_speed = 50 * 176;
        u16 w_speed = 16 * 176;
        u16 bufsize = 2048;
        data.push_back(0x2a); // header: page no
        data.push_back(0x14); // header: page size
        data.push_back(0x3b); // cd-r and cd-rw
        data.push_back(0x00); // writing not supported
        data.push_back(0x7f); // audio
        data.push_back(0xff); // cd flags
        data.push_back(0x2d); // locking support
        data.push_back(0x00); // no volume or mute
        data.push_back(r_speed >> 8);
        data.push_back(r_speed & 0xff);
        data.push_back(0x00);
        data.push_back(0x02); // two volume levels
        data.push_back(bufsize >> 8);
        data.push_back(bufsize & 0xff);
        data.push_back(r_speed >> 8);
        data.push_back(r_speed & 0xff);
        data.push_back(w_speed >> 8);
        data.push_back(w_speed & 0xff);
        data.push_back(w_speed >> 8);
        data.push_back(w_speed & 0xff);
        return;
    }

    default:
        if (!all)
            log_warn("scsi mode page 0x%02hhx not implemented", page);
        return;
    }
}

} // namespace block
} // namespace vcml
