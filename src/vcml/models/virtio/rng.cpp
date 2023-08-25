/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/rng.h"

namespace vcml {
namespace virtio {

void rng::identify(virtio_device_desc& desc) {
    reset();
    desc.device_id = VIRTIO_DEVICE_RNG;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.pci_class = PCI_CLASS_OTHERS;
    desc.request_virtqueue(VIRTQUEUE_REQUEST, 8);
}

bool rng::notify(u32 vqid) {
    vq_message msg;
    int count = 0;

    while (virtio_in->get(vqid, msg)) {
        log_debug("received message from virtqueue %u with %u bytes", vqid,
                  msg.length());

        vector<u8> random(msg.length_out());
        if (pseudo) {
            for (auto& elem : random)
                elem = rand();
        } else {
            mwr::fill_random(random.data(), random.size());
        }

        count++;
        msg.copy_out(random);

        if (!virtio_in->put(vqid, msg))
            return false;
    }

    if (!count)
        log_warn("notify without messages");

    return true;
}

void rng::read_features(u64& features) {
    features = 0;
}

bool rng::write_features(u64 features) {
    return true;
}

bool rng::read_config(const range& addr, void* ptr) {
    return false;
}

bool rng::write_config(const range& addr, const void* ptr) {
    return false;
}

rng::rng(const sc_module_name& nm):
    module(nm),
    virtio_device(),
    virtio_in("virtio_in"),
    pseudo("pseudo", false),
    seed("seed", 0) {
}

rng::~rng() {
    // nothing to do
}

void rng::reset() {
    if (pseudo)
        srand(seed);
}

VCML_EXPORT_MODEL(vcml::virtio::rng, name, args) {
    return new rng(name);
}

} // namespace virtio
} // namespace vcml
