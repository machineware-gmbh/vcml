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

#ifndef VCML_VIRTIO_RNG_H
#define VCML_VIRTIO_RNG_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/virtio.h"

#include "vcml/module.h"

namespace vcml { namespace virtio {

    class rng : public module, public virtio_device
    {
    private:
        enum virtqueues : int {
            VIRTQUEUE_REQUEST = 0,
        };

        unsigned int m_seed;

        virtual void identify(virtio_device_desc& desc) override;
        virtual bool notify(u32 vqid) override;

        virtual void read_features(u64& features) override;
        virtual bool write_features(u64 features) override;

        virtual bool read_config(const range& addr, void* ptr) override;
        virtual bool write_config(const range& addr, const void* ptr) override;

    public:
        virtio_target_socket VIRTIO_IN;

        rng(const sc_module_name& nm);
        virtual ~rng();
        VCML_KIND(virtio::rng);

        virtual void reset();
    };

}}

#endif
