/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/models/virtio/rng.h"

namespace vcml { namespace virtio {

    void rng::identify(virtio_device_desc& desc) {
        reset();
        desc.device_id = VIRTIO_DEVICE_RNG;
        desc.vendor_id = VIRTIO_VENDOR_VCML;
        desc.request_virtqueue(VIRTQUEUE_REQUEST, 8);
    }

    bool rng::notify(u32 vqid) {
        vq_message msg;
        int count = 0;

        while (VIRTIO_IN->get(vqid, msg)) {
            log_debug("received message from virtqueue %u with %u bytes",
                      vqid, msg.length());

            vector<u8> random(msg.length_out);
            for (auto& elem : random)
                elem = rand_r(&m_seed);

            count++;
            msg.copy_out(random);

            if (!VIRTIO_IN->put(vqid, msg))
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
        m_seed(0),
        VIRTIO_IN("VIRTIO_IN") {
    }

    rng::~rng() {
        // nothing to do
    }

    void rng::reset() {
        m_seed = (unsigned int)sc_time_stamp().value();
    }

}}
