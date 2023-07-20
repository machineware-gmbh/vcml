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
        for (auto& elem : random)
            elem = rand_r(&m_seed);

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
    module(nm), virtio_device(), m_seed(0), virtio_in("virtio_in") {
}

rng::~rng() {
    // nothing to do
}

void rng::reset() {
    m_seed = (unsigned int)sc_time_stamp().value();
}

VCML_EXPORT_MODEL(vcml::virtio::rng, name, args) {
    return new rng(name);
}

} // namespace virtio
} // namespace vcml
