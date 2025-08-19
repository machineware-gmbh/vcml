/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BLOCK_SCSI_H
#define VCML_BLOCK_SCSI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/models/block/disk.h"

namespace vcml {
namespace block {

enum scsi_command : u8 {
    SCSI_TEST_UNIT_READY = 0x00,
    SCSI_REZERO_UNIT = 0x01,
    SCSI_REQUEST_SENSE = 0x03,
    SCSI_FORMAT_UNIT = 0x04,
    SCSI_SEEK_10 = 0x10,
    SCSI_INQUIRY = 0x12,
    SCSI_MODE_SENSE_6 = 0x1a,
    SCSI_START_STOP = 0x1b,
    SCSI_SEND_DIAGNOSTIC = 0x1d,
    SCSI_ALLOW_MEDIUM_REMOVAL = 0x1e,
    SCSI_READ_FORMAT_CAPACITY = 0x23,
    SCSI_READ_CAPACITY = 0x25,
    SCSI_READ_10 = 0x28,
    SCSI_WRITE_10 = 0x2a,
    SCSI_WRITE_AND_VERIFY = 0x2e,
    SCSI_VERIFY = 0x2f,
    SCSI_SYNC_CACHE = 0x35,
    SCSI_MODE_SELECT = 0x55,
    SCSI_MODE_SENSE_10 = 0x5a,
    SCSI_READ_12 = 0xa8,
    SCSI_WRITE_12 = 0xaa,
};

const char* scsi_command_str(u8 opcode);

bool scsi_command_transfers_to_device(u8 opcode);
bool scsi_command_transfers_from_device(u8 opcode);

struct scsi_request {
    u8 command[16];
    vector<u8> payload;
};

enum scsi_response : u8 {
    SCSI_GOOD = 0x00,
    SCSI_CHECK_CONDITION = 0x02,
    SCSI_CONDITION_MET = 0x04,
    SCSI_BUSY = 0x08,
    SCSI_INTERMEDIATE = 0x10,
    SCSI_INTERMEDIATE_COND_MET = 0x14,
    SCSI_RESERVATION_CONFLICT = 0x18,
    SCSI_COMMAND_TERMINATED = 0x22,
    SCSI_TASK_SET_FULL = 0x28,
    SCSI_ACA_ACTIVE = 0x30,
    SCSI_TASK_ABORTED = 0x40,
};

const char* scsi_response_str(u8 resp);

constexpr bool success(scsi_response resp) {
    return resp == SCSI_GOOD;
}
constexpr bool failed(scsi_response resp) {
    return resp != SCSI_GOOD;
}

enum scsi_sense_key : u8 {
    SCSI_NO_SENSE = 0x00,
    SCSI_RECOVERED_ERROR = 0x01,
    SCSI_NOT_READY = 0x02,
    SCSI_MEDIUM_ERROR = 0x03,
    SCSI_HARDWARE_ERROR = 0x04,
    SCSI_ILLEGAL_REQUEST = 0x05,
    SCSI_UNIT_ATTENTION = 0x06,
    SCSI_DATA_PROTECT = 0x07,
    SCSI_BLANK_CHECK = 0x08,
    SCSI_COPY_ABORTED = 0x0a,
    SCSI_ABORTED_COMMAND = 0x0b,
    SCSI_VOLUME_OVERFLOW = 0x0d,
    SCSI_MISCOMPARE = 0x0e,
};

struct scsi_sense {
    u8 key;
    u8 asc;
    u8 ascq;
};

inline bool operator==(const scsi_sense& a, const scsi_sense& b) {
    return a.key == b.key && a.asc == b.asc && a.ascq == b.ascq;
}

inline bool operator!=(const scsi_sense& a, const scsi_sense& b) {
    return a.key != b.key || a.asc != b.asc || a.ascq != b.ascq;
}

extern const scsi_sense SENSE_NOTHING;
extern const scsi_sense SENSE_NOT_READY;
extern const scsi_sense SENSE_NO_MEDIUM;
extern const scsi_sense SENSE_MEDIUM_ERROR;
extern const scsi_sense SENSE_ILLEGAL_REQ;
extern const scsi_sense SENSE_UNIT_ATTENTION;
extern const scsi_sense SENSE_DATA_PROTECT;

enum scsi_mode_page : u8 {
    SCSI_MODE_PAGE_RW_ERROR = 0x01,
    SCSI_MODE_PAGE_CACHING = 0x08,
    SCSI_MODE_PAGE_CONTROL = 0x0a,
    SCSI_MODE_PAGE_EXCEPTIONS = 0x1c,
    SCSI_MODE_PAGE_ALL_PAGES = 0x3f,
};

class scsi_disk : public disk
{
private:
    scsi_sense m_sense;

public:
    property<size_t> blockbits;

    property<string> vendor;
    property<string> product;
    property<string> revision;

    const scsi_sense& get_sense() const { return m_sense; }
    void set_sense(const scsi_sense& s) { m_sense = s; }

    size_t blocksize() const { return 1ull << min<size_t>(blockbits, 15); }

    scsi_disk(const sc_module_name& nm, const string& image = "",
              bool readonly = false, bool writeignore = false);
    virtual ~scsi_disk();
    VCML_KIND(block::scsi_disk);

    virtual scsi_response scsi_handle_command(scsi_request& req);
    virtual void scsi_write_mode_page(vector<u8>& data, u8 page, u8 control,
                                      bool warn);
};

} // namespace block
} // namespace vcml

#endif
