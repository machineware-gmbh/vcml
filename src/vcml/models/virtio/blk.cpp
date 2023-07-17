/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/blk.h"

namespace vcml {
namespace virtio {

enum virtqueues : int {
    VIRTQUEUE_REQUEST = 0,
    VIRTQUEUE0_LENGTH = 256,
};

enum : size_t {
    SECTOR_BITS = 9,
    SECTOR_SIZE = 1u << 9,
};

enum features : u64 {
    VIRTIO_BLK_F_SIZE_MAX = bit(1),
    VIRTIO_BLK_F_SEG_MAX = bit(2),
    VIRTIO_BLK_F_GEOMETRY = bit(4),
    VIRTIO_BLK_F_RO = bit(5),
    VIRTIO_BLK_F_BLK_SIZE = bit(6),
    VIRTIO_BLK_F_FLUSH = bit(9),
    VIRTIO_BLK_F_TOPOLOGY = bit(10),
    VIRTIO_BLK_F_CONFIG_WCE = bit(11),
    VIRTIO_BLK_F_DISCARD = bit(13),
    VIRTIO_BLK_F_WRITE_ZEROES = bit(14),
};

enum virtio_blk_type : u32 {
    VIRTIO_BLK_T_IN = 0,
    VIRTIO_BLK_T_OUT = 1,
    VIRTIO_BLK_T_FLUSH = 4,
    VIRTIO_BLK_T_GET_ID = 8,
    VIRTIO_BLK_T_DISCARD = 11,
    VIRTIO_BLK_T_WRITE_ZEROES = 13,
};

enum virtio_blk_status : u8 {
    VIRTIO_BLK_S_OK = 0,
    VIRTIO_BLK_S_IOERR = 1,
    VIRTIO_BLK_S_UNSUPP = 2,
};

enum virtio_blk_flags : u32 {
    VIRTIO_BLK_FLAG_UNMAP = bit(0),
};

struct virtio_blk_dwz {
    u64 sector;
    u32 num_sectors;
    u32 flags;
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
    case VIRTIO_BLK_T_IN:
        return process_in(req, msg);

    case VIRTIO_BLK_T_OUT:
        return process_out(req, msg);

    case VIRTIO_BLK_T_FLUSH:
        return process_flush(req, msg);

    case VIRTIO_BLK_T_GET_ID:
        return process_get_id(req, msg);

    case VIRTIO_BLK_T_DISCARD:
        return process_discard(req, msg);

    case VIRTIO_BLK_T_WRITE_ZEROES:
        return process_write_zeroes(req, msg);

    default:
        log_warn("unsupported request: %u", req.type);
        put_status(msg, VIRTIO_BLK_S_UNSUPP);
        return true;
    }
}

bool blk::process_in(virtio_blk_req& req, vq_message& msg) {
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

bool blk::process_out(virtio_blk_req& req, vq_message& msg) {
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

bool blk::process_flush(virtio_blk_req& req, vq_message& msg) {
    log_debug("flush disk request");
    if (!disk.flush()) {
        log_warn("disk flush request failed");
        put_status(msg, VIRTIO_BLK_S_IOERR);
        return true;
    }

    put_status(msg, VIRTIO_BLK_S_OK);
    return true;
}

bool blk::process_get_id(virtio_blk_req& req, vq_message& msg) {
    log_debug("get_id request");
    char buffer[20] = {};
    snprintf(buffer, sizeof(buffer), "%s", disk.serial.get().c_str());
    msg.copy_in(buffer);
    put_status(msg, VIRTIO_BLK_S_OK);
    return true;
}

bool blk::process_discard(virtio_blk_req& req, vq_message& msg) {
    virtio_blk_dwz dwz;
    if (msg.copy_in(dwz, sizeof(req)) != sizeof(dwz)) {
        log_error("unable to read discard arguments");
        return false;
    }

    size_t length = dwz.num_sectors * SECTOR_SIZE;
    log_debug("discard sector %llu, %zu bytes", dwz.sector, length);
    put_status(msg, VIRTIO_BLK_S_OK);
    return true;
}

bool blk::process_write_zeroes(virtio_blk_req& req, vq_message& msg) {
    virtio_blk_dwz dwz;
    if (msg.copy_in(dwz, sizeof(req)) != sizeof(dwz)) {
        log_error("unable to read discard arguments");
        return false;
    }

    if (!disk.seek(dwz.sector * SECTOR_SIZE)) {
        log_warn("seek request failed for sector %llu", dwz.sector);
        put_status(msg, VIRTIO_BLK_S_IOERR);
        return true;
    }

    size_t length = dwz.num_sectors * SECTOR_SIZE;
    log_debug("write zeros to sector %llu, %zu bytes", dwz.sector, length);
    if (!disk.wzero(length, dwz.flags & VIRTIO_BLK_FLAG_UNMAP)) {
        log_warn("write zero request failed for sector %llu", dwz.sector);
        put_status(msg, VIRTIO_BLK_S_IOERR);
        return true;
    }

    put_status(msg, VIRTIO_BLK_S_OK);
    return true;
}

void blk::identify(virtio_device_desc& desc) {
    reset();
    desc.device_id = VIRTIO_DEVICE_BLOCK;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.pci_class = PCI_CLASS_STORAGE_SCSI;
    desc.request_virtqueue(VIRTQUEUE_REQUEST, VIRTQUEUE0_LENGTH);
}

bool blk::notify(u32 vqid) {
    vq_message msg;

    while (virtio_in->get(vqid, msg)) {
        log_debug("received message from virtqueue %u with %u bytes", vqid,
                  msg.length());

        process_command(msg);

        if (!virtio_in->put(vqid, msg))
            return false;
    }

    return true;
}

void blk::read_features(u64& features) {
    features = VIRTIO_BLK_F_FLUSH;
    if (disk.readonly)
        features |= VIRTIO_BLK_F_RO;
    if (m_config.max_discard_sectors)
        features |= VIRTIO_BLK_F_DISCARD;
    if (m_config.max_write_zeroes_sectors)
        features |= VIRTIO_BLK_F_WRITE_ZEROES;
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
    max_size("max_size", 4096),
    max_discard_sectors("max_discard_sectors", 4096),
    max_write_zeroes_sectors("max_write_zeroes_sectors", 4096),
    disk("disk", image, readonly),
    virtio_in("virtio_in") {
    m_config.capacity = disk.capacity() / SECTOR_SIZE;
    m_config.blk_size = 512;
    m_config.size_max = max_size;
    m_config.seg_max = VIRTQUEUE0_LENGTH - 2;
    m_config.max_discard_sectors = max_discard_sectors;
    m_config.max_discard_seg = 1;
    m_config.discard_sector_alignment = m_config.blk_size >> SECTOR_BITS;
    m_config.max_write_zeroes_sectors = max_write_zeroes_sectors;
    m_config.max_write_zeroes_seg = 1;
    m_config.write_zeroes_may_unmap = true;
}

blk::~blk() {
    // nothing to do
}

void blk::reset() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::virtio::blk, name, args) {
    return new blk(name);
}

} // namespace virtio
} // namespace vcml
