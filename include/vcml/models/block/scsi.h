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
    SCSI_WRITE_SAME_10 = 0x41,
    SCSI_MODE_SELECT = 0x55,
    SCSI_MODE_SENSE_10 = 0x5a,
    SCSI_WRITE_SAME_16 = 0x93,
    SCSI_READ_CAPACITY_16 = 0x9e,
    SCSI_REPORT_LUNS = 0xa0,
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
extern const scsi_sense SENSE_ILLEGAL_FIELD;
extern const scsi_sense SENSE_ILLEGAL_PARAM;
extern const scsi_sense SENSE_UNIT_ATTENTION;
extern const scsi_sense SENSE_DATA_PROTECT;

struct scsi_block_limits {
    bool wnr;
    u8 max_compare_and_write_length;
    u16 opt_transfer_length_gran;
    u32 max_transfer_length;
    u32 opt_transfer_length;
    u32 max_prefetch_length;
    u32 max_unmap_lba_count;
    u32 max_unmap_blk_desc_count;
    u32 opt_unmap_gran;
    u32 unmap_gran_align;
    u64 max_write_same_len;
    u32 max_atomic_transfer_length;
    u32 atomic_align;
    u32 atomic_transfer_length_gran;
};

enum scsi_device_type : u8 {
    SCSI_DEVICE_DIRECT_ACCESS = 0x00,
    SCSI_DEVICE_SEQUENTIAL_ACCESS = 0x01,
    SCSI_DEVICE_PRINTER = 0x02,
    SCSI_DEVICE_CD_DVD = 0x05,
    SCSI_DEVICE_SCANNER = 0x06,
    SCSI_DEVICE_WLUN = 0x1e,
};

enum scsi_mode_page : u8 {
    SCSI_MODE_PAGE_RW_ERROR = 0x01,
    SCSI_MODE_PAGE_CACHING = 0x08,
    SCSI_MODE_PAGE_CONTROL = 0x0a,
    SCSI_MODE_PAGE_EXCEPTIONS = 0x1c,
    SCSI_MODE_PAGE_CAPABILITIES = 0x2a,
    SCSI_MODE_PAGE_ALL_PAGES = 0x3f,
};

u64 scsi_read(const u8* ptr, size_t len);
void scsi_write(u8* ptr, size_t len, u64 data);
void scsi_write_str(u8* buf, size_t len, const string& str);
void scsi_write_sense(u8* buf, size_t len, const scsi_sense& sense);
void scsi_write_block_limits(u8* buf, size_t len, const scsi_block_limits& bl);

class scsi_disk : public disk
{
private:
    scsi_sense m_sense;

    scsi_response scsi_inquiry(scsi_request& req);
    scsi_response scsi_inquiry_vpd(scsi_request& req);
    scsi_response scsi_report_luns(scsi_request& req);
    scsi_response scsi_request_sense(scsi_request& req);
    scsi_response scsi_mode_sense(scsi_request& req, bool is10);
    scsi_response scsi_read_capacity(scsi_request& req, bool is16);
    scsi_response scsi_handle_seek(scsi_request& req);
    scsi_response scsi_handle_read(scsi_request& req, bool is12);
    scsi_response scsi_handle_write(scsi_request& req, bool is12);
    scsi_response scsi_write_same(scsi_request& req, bool is16);

public:
    property<bool> removable;

    property<size_t> blockbits;

    property<u64> device_wwn;
    property<u64> port_wwn;
    property<u32> port_idx;

    property<string> vendor;
    property<string> product;
    property<string> revision;

    const scsi_sense& get_sense() const { return m_sense; }
    void set_sense(const scsi_sense& s) { m_sense = s; }

    size_t blocksize() const { return 1ull << min<size_t>(blockbits, 15); }

    scsi_disk(const sc_module_name& nm, const string& image = "",
              bool readonly = false, bool writeignore = false,
              bool removable = false);
    virtual ~scsi_disk();
    VCML_KIND(block::scsi_disk);

    virtual scsi_response scsi_handle_command(scsi_request& req);
    virtual void scsi_write_mode_page(vector<u8>& data, u8 page, u8 control,
                                      bool all);
};

} // namespace block
} // namespace vcml

#endif
