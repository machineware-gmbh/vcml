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
struct rst_payload;
struct clk_payload;
struct pci_payload;
struct i2c_payload;
struct spi_payload;
struct sd_command;
struct sd_data;
struct vq_message;
struct serial_payload;

enum trace_direction : int {
    TRACE_FW          = 1,
    TRACE_FW_NOINDENT = 0,
    TRACE_BW_NOINDENT = -1,
    TRACE_BW          = -2,
};

inline bool is_forward_trace(trace_direction dir) {
    return dir >= 0;
}
inline bool is_backward_trace(trace_direction dir) {
    return dir < 0;
}

enum protocol_kind {
    PROTO_TLM,
    PROTO_IRQ,
    PROTO_RST,
    PROTO_CLK,
    PROTO_PCI,
    PROTO_I2C,
    PROTO_SPI,
    PROTO_SD,
    PROTO_SERIAL,
    PROTO_VIRTIO,
    NUM_PROTOCOLS,
};

const char* protocol_name(protocol_kind kind);

template <typename PAYLOAD>
struct protocol {};

template <>
struct protocol<tlm_generic_payload> {
    static constexpr protocol_kind KIND = PROTO_TLM;
};

template <>
struct protocol<irq_payload> {
    static constexpr protocol_kind KIND = PROTO_IRQ;
};

template <>
struct protocol<rst_payload> {
    static constexpr protocol_kind KIND = PROTO_RST;
};

template <>
struct protocol<clk_payload> {
    static constexpr protocol_kind KIND = PROTO_CLK;
};

template <>
struct protocol<pci_payload> {
    static constexpr protocol_kind KIND = PROTO_PCI;
};

template <>
struct protocol<i2c_payload> {
    static constexpr protocol_kind KIND = PROTO_I2C;
};

template <>
struct protocol<spi_payload> {
    static constexpr protocol_kind KIND = PROTO_SPI;
};

template <>
struct protocol<sd_command> {
    static constexpr protocol_kind KIND = PROTO_SD;
};

template <>
struct protocol<sd_data> {
    static constexpr protocol_kind KIND = PROTO_SD;
};

template <>
struct protocol<serial_payload> {
    static constexpr protocol_kind KIND = PROTO_SERIAL;
};

template <>
struct protocol<vq_message> {
    static constexpr protocol_kind KIND = PROTO_VIRTIO;
};

class tracer
{
public:
    template <typename PAYLOAD>
    struct activity {
        protocol_kind kind;
        trace_direction dir;
        bool error;
        const sc_object& port;
        const PAYLOAD& payload;
        const sc_time& t;
        const u64 cycle;
    };

    virtual void trace(const activity<tlm_generic_payload>&) = 0;
    virtual void trace(const activity<irq_payload>&)         = 0;
    virtual void trace(const activity<rst_payload>&)         = 0;
    virtual void trace(const activity<clk_payload>&)         = 0;
    virtual void trace(const activity<pci_payload>&)         = 0;
    virtual void trace(const activity<i2c_payload>&)         = 0;
    virtual void trace(const activity<spi_payload>&)         = 0;
    virtual void trace(const activity<sd_command>&)          = 0;
    virtual void trace(const activity<sd_data>&)             = 0;
    virtual void trace(const activity<vq_message>&)          = 0;
    virtual void trace(const activity<serial_payload>&)      = 0;

    tracer();
    virtual ~tracer();

    template <typename PAYLOAD>
    static void record(trace_direction dir, const sc_object& port,
                       const PAYLOAD& payload,
                       const sc_time& t = SC_ZERO_TIME) {
        auto& tracers = tracer::all();
        if (!tracers.empty()) {
            const tracer::activity<PAYLOAD> msg = {
                protocol<PAYLOAD>::KIND,
                dir,
                failed(payload),
                port,
                payload,
                t + sc_time_stamp(),
                sc_delta_count(),
            };

            for (tracer* tr : tracers)
                tr->trace(msg);
        }
    }

    static bool any() { return !all().empty(); }

protected:
    template <typename PAYLOAD>
    static void print_timing(ostream& os, const activity<PAYLOAD>& msg) {
        print_timing(os, msg.t, msg.cycle);
    }

    static void print_timing(ostream& os, const sc_time& time, u64 delta);

    static unordered_set<tracer*>& all();
};

} // namespace vcml

#endif
