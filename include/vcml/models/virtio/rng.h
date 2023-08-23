/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_RNG_H
#define VCML_VIRTIO_RNG_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/virtio.h"

namespace vcml {
namespace virtio {

class rng : public module, public virtio_device
{
private:
    enum virtqueues : int {
        VIRTQUEUE_REQUEST = 0,
    };

    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

public:
    virtio_target_socket virtio_in;

    property<bool> pseudo;
    property<u32> seed;

    rng(const sc_module_name& nm);
    virtual ~rng();
    VCML_KIND(virtio::rng);

    virtual void reset();
};

} // namespace virtio
} // namespace vcml

#endif
