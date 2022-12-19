/******************************************************************************
 *                                                                            *
 * Copyright 2022 MachineWare GmbH                                            *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/blk.h"

namespace vcml {
namespace virtio {

enum : size_t {
    SECTOR_SIZE = 512,
};

enum virtio_blk_type : u32 {
    VIRTIO_BLK_T_IN = 0,
    VIRTIO_BLK_T_OUT = 1,
    VIRTIO_BLK_T_FLUSH = 4,
    VIRTIO_BLK_T_DISCARD = 11,
    VIRTIO_BLK_T_WRITE_ZEROES = 13,
};

enum virtio_blk_status : u8 {
    VIRTIO_BLK_S_OK = 0,
    VIRTIO_BLK_S_IOERR = 1,
    VIRTIO_BLK_S_UNSUPP = 2,
};

struct virtio_blk_req {
    u32 type;
    u32 reserved;
    u64 sector;
};

static void put_status(vq_message& msg, u8 status) {
    size_t sz = msg.length_out();
    msg.copy_out(status, sz - 1);
}

bool blk::process_command(vq_message& msg) {
    virtio_blk_req req = {};
    if (msg.length_in() < sizeof(req) || msg.length_out() < sizeof(u8)) {
        log_error("message does not hold required request fields");
        return false;
    }

    if (msg.copy_in(req) != sizeof(req)) {
        log_error("unable to read request");
        return false;
    }

    switch (req.type) {
    case VIRTIO_BLK_T_IN: {
        size_t length = msg.length_out() - 1;
        log_debug("read sector %llu, %zu bytes", req.sector, length);
        if (length % SECTOR_SIZE) {
            log_warn("data length is not a multiple of sector size");
            put_status(msg, VIRTIO_BLK_S_IOERR);
            return true;
        }

        if (!disk.seek(req.sector * SECTOR_SIZE)) {
            log_warn("seek request failed for sector %llu", req.sector);
            put_status(msg, VIRTIO_BLK_S_IOERR);
            return true;
        }

        u8 sector[SECTOR_SIZE];
        for (size_t count = 0; count < length; count += SECTOR_SIZE) {
            if (!disk.read(sector, SECTOR_SIZE)) {
                log_warn("disk read request failed");
                put_status(msg, VIRTIO_BLK_S_IOERR);
                return true;
            }

            msg.copy_out(sector, count);
        }

        put_status(msg, VIRTIO_BLK_S_OK);
        return true;
    }

    case VIRTIO_BLK_T_OUT: {
        size_t length = msg.length_in() - sizeof(req);
        log_debug("write sector %llu, %zu bytes", req.sector, length);
        if (length % SECTOR_SIZE) {
            log_warn("data length is not a multiple of sector size");
            put_status(msg, VIRTIO_BLK_S_IOERR);
            return true;
        }

        if (!disk.seek(req.sector * SECTOR_SIZE)) {
            log_warn("seek request failed for sector %llu", req.sector);
            put_status(msg, VIRTIO_BLK_S_IOERR);
            return true;
        }

        u8 sector[SECTOR_SIZE];
        for (size_t count = 0; count < length; count += SECTOR_SIZE) {
            msg.copy_in(sector, count + sizeof(req));

            if (!disk.write(sector, SECTOR_SIZE)) {
                log_warn("disk write request failed");
                put_status(msg, VIRTIO_BLK_S_IOERR);
                return true;
            }
        }

        put_status(msg, VIRTIO_BLK_S_OK);
        return true;
    }

    case VIRTIO_BLK_T_FLUSH: {
        log_debug("flush disk request");
        put_status(msg, VIRTIO_BLK_S_OK);
        return true;
    }

    default:
        log_warn("unsupported request: %u", req.type);
        put_status(msg, VIRTIO_BLK_S_UNSUPP);
        return true;
    }
}

void blk::identify(virtio_device_desc& desc) {
    reset();
    desc.device_id = VIRTIO_DEVICE_BLOCK;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.pci_class = PCI_CLASS_STORAGE_SCSI;
    desc.request_virtqueue(VIRTQUEUE_REQUEST, 256);
}

bool blk::notify(u32 vqid) {
    vq_message msg;
    int count = 0;

    while (virtio_in->get(vqid, msg)) {
        log_debug("received message from virtqueue %u with %u bytes", vqid,
                  msg.length());

        process_command(msg);
        count++;

        if (!virtio_in->put(vqid, msg))
            return false;
    }

    if (!count)
        log_warn("notify without messages");

    return true;
}

void blk::read_features(u64& features) {
    features = 0;
    features |= VIRTIO_BLK_F_FLUSH;
    features |= VIRTIO_BLK_F_DISCARD;
    features |= VIRTIO_BLK_F_WRITE_ZEROES;

    if (disk.readonly)
        features |= VIRTIO_BLK_F_RO;
    if (m_config.size_max)
        features |= VIRTIO_BLK_F_SIZE_MAX;
    if (m_config.seg_max)
        features |= VIRTIO_BLK_F_SEG_MAX;
    if (m_config.blk_size)
        features |= VIRTIO_BLK_F_BLK_SIZE;
    if (m_config.geometry.sectors)
        features |= VIRTIO_BLK_F_GEOMETRY;
    if (m_config.topology.min_io_size)
        features |= VIRTIO_BLK_F_TOPOLOGY;
}

bool blk::write_features(u64 features) {
    return disk.readonly == !!(features & VIRTIO_BLK_F_RO);
}

bool blk::read_config(const range& addr, void* ptr) {
    if (addr.end >= sizeof(m_config))
        return false;

    memcpy(ptr, (u8*)&m_config + addr.start, addr.length());
    return true;
}

bool blk::write_config(const range& addr, const void* ptr) {
    return false;
}

blk::blk(const sc_module_name& nm):
    module(nm),
    virtio_device(),
    m_config(),
    image("image", ""),
    readonly("readonly", false),
    disk("disk", image, readonly),
    virtio_in("virtio_in") {
    m_config.capacity = disk.capacity() / SECTOR_SIZE;
    m_config.blk_size = 512;

    // m_config.size_max;
    // m_config.seg_max;
    // m_config.geometry.cylinders;
    // m_config.geometry.heads;
    // m_config.geometry.sectors;
    // m_config.topology.physical_block_exp;
    // m_config.topology.alignment_offset;
    // m_config.topology.min_io_size;
    // m_config.topology.opt_io_size;
    // m_config.writeback;
    // m_config.max_discard_sectors;
    // m_config.max_discard_seg;
    // m_config.discard_sector_alignment;
    // m_config.max_write_zeroes_sectors;
    // m_config.max_write_zeroes_seg;
    // m_config.write_zeroes_may_unmap;
}

blk::~blk() {
    // nothing to do
}

void blk::reset() {
    // nothing to do
}

} // namespace virtio
} // namespace vcml
