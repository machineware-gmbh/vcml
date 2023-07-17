/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_BLK_H
#define VCML_VIRTIO_BLK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/virtio.h"
#include "vcml/models/block/disk.h"

namespace vcml {
namespace virtio {

class blk : public module, public virtio_device
{
private:
    struct virtio_blk_req {
        u32 type;
        u32 reserved;
        u64 sector;
    };

    struct config {
        u64 capacity;
        u32 size_max;
        u32 seg_max;

        struct geometry {
            u16 cylinders;
            u8 heads;
            u8 sectors;
        } geometry;

        u32 blk_size;

        struct topology {
            u8 physical_block_exp;
            u8 alignment_offset;
            u16 min_io_size;
            u32 opt_io_size;
        } topology;

        u8 writeback;
        u8 unused0[3];
        u32 max_discard_sectors;
        u32 max_discard_seg;
        u32 discard_sector_alignment;
        u32 max_write_zeroes_sectors;
        u32 max_write_zeroes_seg;
        u8 write_zeroes_may_unmap;
        u8 unused1[3];
    } m_config;

    bool process_command(vq_message& msg);
    bool process_in(virtio_blk_req& req, vq_message& msg);
    bool process_out(virtio_blk_req& req, vq_message& msg);
    bool process_flush(virtio_blk_req& req, vq_message& msg);
    bool process_get_id(virtio_blk_req& req, vq_message& msg);
    bool process_discard(virtio_blk_req& req, vq_message& msg);
    bool process_write_zeroes(virtio_blk_req& req, vq_message& msg);

    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

public:
    property<string> image;
    property<bool> readonly;

    property<u32> max_size;
    property<u32> max_discard_sectors;
    property<u32> max_write_zeroes_sectors;

    block::disk disk;

    virtio_target_socket virtio_in;

    blk(const sc_module_name& nm);
    virtual ~blk();
    VCML_KIND(virtio::blk);

    virtual void reset();
};

} // namespace virtio
} // namespace vcml

#endif
