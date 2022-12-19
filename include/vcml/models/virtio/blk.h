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

#ifndef VCML_VIRTIO_BLK_H
#define VCML_VIRTIO_BLK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/module.h"

#include "vcml/protocols/virtio.h"
#include "vcml/models/block/disk.h"

namespace vcml {
namespace virtio {

class blk : public module, public virtio_device
{
private:
    enum virtqueues : int {
        VIRTQUEUE_REQUEST = 0,
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

    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

public:
    property<string> image;
    property<bool> readonly;

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
