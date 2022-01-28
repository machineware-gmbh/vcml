/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_TRACER_H
#define VCML_TRACER_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    struct irq_payload;
    struct pci_payload;
    struct spi_payload;
    struct sd_command;
    struct sd_data;
    struct vq_message;

    enum trace_direction : int {
        TRACE_FW = 1,
        TRACE_FW_NOINDENT = 0,
        TRACE_BW_NOINDENT = -1,
        TRACE_BW = -2,
    };

    inline bool is_forward_trace(trace_direction dir)  { return dir >= 0; }
    inline bool is_backward_trace(trace_direction dir) { return dir < 0; }

    enum protocol_kind {
        PROTO_TLM,
        PROTO_IRQ,
        PROTO_PCI,
        PROTO_SPI,
        PROTO_SD,
        PROTO_VIRTIO,
        NUM_PROTOCOLS,
    };

    const char* protocol_name(protocol_kind kind);

    template <typename PAYLOAD>
    struct protocol {};

    template <> struct protocol<tlm_generic_payload> {
        static constexpr protocol_kind kind = PROTO_TLM;
    };

    template <> struct protocol<irq_payload> {
        static constexpr protocol_kind kind = PROTO_IRQ;
    };

    template <> struct protocol<pci_payload> {
        static constexpr protocol_kind kind = PROTO_PCI;
    };

    template <> struct protocol<spi_payload> {
        static constexpr protocol_kind kind = PROTO_SPI;
    };

    template <> struct protocol<sd_command> {
        static constexpr protocol_kind kind = PROTO_SD;
    };

    template <> struct protocol<sd_data> {
        static constexpr protocol_kind kind = PROTO_SD;
    };

    template <> struct protocol<vq_message> {
        static constexpr protocol_kind kind = PROTO_VIRTIO;
    };

    class tracer
    {
    public:
        template <typename PAYLOAD>
        struct entry {
            protocol_kind kind;
            trace_direction dir;
            bool error;
            const sc_object& port;
            const PAYLOAD& payload;
            const sc_time& t;
            const u64 cycle;
        };

        virtual void trace(const entry<tlm_generic_payload>&) {}
        virtual void trace(const entry<irq_payload>&) {}
        virtual void trace(const entry<pci_payload>&) {}
        virtual void trace(const entry<spi_payload>&) {}
        virtual void trace(const entry<sd_command>&) {}
        virtual void trace(const entry<sd_data>&) {}
        virtual void trace(const entry<vq_message>&) {}

        tracer();
        virtual ~tracer();

        template <typename PAYLOAD>
        static void record(trace_direction dir, const sc_object& port,
            const PAYLOAD& payload, const sc_time& t = SC_ZERO_TIME) {
            const tracer::entry<PAYLOAD> msg = {
                kind:    protocol<PAYLOAD>::kind,
                dir:     dir,
                error:   failed(payload),
                port:    port,
                payload: payload,
                t:       t + sc_time_stamp(),
                cycle:   sc_delta_count(),
            };

            for (tracer* tr : tracer::all())
                tr->trace(msg);
        }

        template <typename PAYLOAD>
        static void print_timing(ostream& os, const entry<PAYLOAD>& msg) {
            print_timing(os, msg.t, msg.cycle);
        }

        static void print_timing(ostream& os, const sc_time& time, u64 delta);

        static unordered_set<tracer*>& all();
    };

}

#endif
