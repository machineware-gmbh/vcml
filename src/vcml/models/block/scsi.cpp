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

namespace vcml {
namespace block {

static u64 scsi_read(const u8* ptr, size_t len) {
    VCML_ERROR_ON(len > 8, "attempt to read more than 8 bytes");

    u64 result = 0;
    for (size_t i = 0; i < len; i++) {
#ifdef MWR_HOST_LITTLE_ENDIAN
        result |= (u64)ptr[i] << ((len - i - 1) * 8);
#else
        result |= (u64)data[i] << (i * 8);
#endif
    }

    return result;
}

static void scsi_write(u8* ptr, size_t len, u64 data) {
    VCML_ERROR_ON(len > 8, "attempt to write more than 8 bytes");
    for (size_t i = 0; i < len; i++) {
#ifdef MWR_HOST_LITTLE_ENDIAN
        ptr[i] = data >> ((len - i - 1) * 8);
#else
        ptr[i] = data >> (i * 8);
#endif
    }
}

static void scsi_write_str(u8* ptr, size_t len, const string& str) {
    for (size_t i = 0; i < len - 1; i++)
        ptr[i] = i < str.length() ? str[i] : ' ';
    if (len > 0)
        ptr[len - 1] = 0;
}

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
    case SCSI_MODE_SELECT:
        return "MODE_SELECT";
    case SCSI_MODE_SENSE_10:
        return "MODE_SENSE_10";
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
const scsi_sense SENSE_UNIT_ATTENTION{ SCSI_UNIT_ATTENTION, 0x28, 0x00 };
const scsi_sense SENSE_DATA_PROTECT{ SCSI_DATA_PROTECT, 0x27, 0x00 };

scsi_disk::scsi_disk(const sc_module_name& nm, const string& image,
                     bool readonly, bool writeignore):
    disk(nm, image, readonly, writeignore),
    m_sense(SENSE_NOTHING),
    blockbits("blockbits", 9),
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

    case SCSI_REQUEST_SENSE: {
        req.payload.assign(18, 0);
        req.payload[0] = 0xf0;
        req.payload[2] = m_sense.key;
        req.payload[7] = 0x0a;
        req.payload[12] = m_sense.asc;
        req.payload[13] = m_sense.ascq;
        log_debug("request sense [0x%02hhx 0x%02hhx 0x%02hhx]", m_sense.key,
                  m_sense.asc, m_sense.ascq);
        return SCSI_GOOD;
    }

    case SCSI_FORMAT_UNIT: {
        log_debug("format unit");
        return SCSI_GOOD;
    }

    case SCSI_INQUIRY: {
        log_debug("inquire request");
        req.payload.assign(36, 0);
        req.payload[1] = 0x80; // RMB
        req.payload[3] = 0x01; // AERC
        req.payload[4] = 36;   // extra data length
        scsi_write_str(req.payload.data() + 8, 8, vendor);
        scsi_write_str(req.payload.data() + 16, 16, product);
        scsi_write_str(req.payload.data() + 32, 4, revision);
        return SCSI_GOOD;
    }

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

    case SCSI_READ_CAPACITY: {
        if (capacity() == 0) {
            log_debug("read capacity: no medium");
            m_sense = SENSE_NO_MEDIUM;
            return SCSI_CHECK_CONDITION;
        }

        log_debug("read capacity: %zu bytes", capacity());

        size_t bsz = blocksize();
        size_t lba = min(udivup(capacity(), bsz) - 1, (u64)U32_MAX);

        req.payload.assign(8, 0);
        scsi_write(req.payload.data() + 0, 4, lba);
        scsi_write(req.payload.data() + 4, 4, bsz);
        return SCSI_GOOD;
    }

    case SCSI_SEEK_10: {
        size_t off = scsi_read(req.command + 2, 4) * blocksize();
        log_debug("seek offset %zu", off);
        if (seek(off))
            return SCSI_GOOD;

        m_sense = SENSE_MEDIUM_ERROR;
        return SCSI_CHECK_CONDITION;
    }

    case SCSI_READ_10:
    case SCSI_READ_12: {
        size_t pos = req.command[0] == SCSI_READ_12 ? 6 : 7;
        size_t num = req.command[0] == SCSI_READ_12 ? 4 : 2;
        size_t off = scsi_read(req.command + 2, 4) << blockbits;
        size_t len = scsi_read(req.command + pos, num) << blockbits;
        log_debug("read offset %zu, %zu bytes", off, len);
        req.payload.assign(len, 0);
        if (seek(off) && read(req.payload.data(), req.payload.size()))
            return SCSI_GOOD;

        m_sense = SENSE_MEDIUM_ERROR;
        return SCSI_CHECK_CONDITION;
    }

    case SCSI_WRITE_10:
    case SCSI_WRITE_12: {
        size_t pos = req.command[0] == SCSI_WRITE_12 ? 6 : 7;
        size_t num = req.command[0] == SCSI_WRITE_12 ? 4 : 2;
        size_t off = scsi_read(req.command + 2, 4) << blockbits;
        size_t len = scsi_read(req.command + pos, num) << blockbits;
        log_debug("write offset %zu, %zu bytes", off, len);
        if (seek(off) && write(req.payload.data(), req.payload.size()))
            return SCSI_GOOD;

        m_sense = SENSE_MEDIUM_ERROR;
        return SCSI_CHECK_CONDITION;
    }

    case SCSI_MODE_SENSE_6:
    case SCSI_MODE_SENSE_10: {
        u8 size = req.command[0] == SCSI_MODE_SENSE_10 ? 2 : 1;
        u8 page = req.command[2] & 0x3f;
        u8 ctrl = req.command[2] & 0xc0;
        bool dbd = req.command[1] & 0x08;

        if (page == SCSI_MODE_PAGE_ALL_PAGES)
            log_debug("mode sense (all pages)");
        else
            log_debug("mode sense (page 0x%02hhx)", page);

        req.payload.assign(size, 0); // total size placeholder
        req.payload.push_back(0x00); // default media type
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
                scsi_write_mode_page(req.payload, pg, ctrl, false);
        } else {
            scsi_write_mode_page(req.payload, page, ctrl, true);
        }

        // patch in final size at offset 0
        scsi_write(req.payload.data(), size, req.payload.size() - size);
        return SCSI_GOOD;
    }

    default:
        m_sense = SENSE_ILLEGAL_REQ;
        return SCSI_CHECK_CONDITION;
    }
}

void scsi_disk::scsi_write_mode_page(vector<u8>& data, u8 page, u8 control,
                                     bool warn) {
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

    default:
        if (warn)
            log_warn("scsi mode page 0x%02hhx not implemented", page);
        return;
    }
}

} // namespace block
} // namespace vcml
