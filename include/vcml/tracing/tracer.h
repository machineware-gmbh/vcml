/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACER_H
#define VCML_TRACER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

struct gpio_payload;
struct clk_payload;
struct pci_payload;
struct i2c_payload;
struct spi_payload;
struct sd_command;
struct sd_data;
struct vq_message;
struct serial_payload;
struct eth_frame;
struct can_frame;
struct usb_packet;

enum trace_direction : int {
    TRACE_FW = 2,
    TRACE_FW_NOINDENT = 1,
    TRACE_NONE = 0,
    TRACE_BW_NOINDENT = -1,
    TRACE_BW = -2,
};

inline bool is_forward_trace(trace_direction dir) {
    return dir > 0;
}
inline bool is_backward_trace(trace_direction dir) {
    return dir < 0;
}

enum protocol_kind {
    PROTO_TLM,
    PROTO_GPIO,
    PROTO_CLK,
    PROTO_PCI,
    PROTO_I2C,
    PROTO_SPI,
    PROTO_SD,
    PROTO_SERIAL,
    PROTO_VIRTIO,
    PROTO_ETHERNET,
    PROTO_CAN,
    PROTO_USB,
    NUM_PROTOCOLS,
};

const char* protocol_name(protocol_kind kind);

template <typename PAYLOAD>
struct protocol {};

template <>
struct protocol<tlm_generic_payload> {
    static constexpr protocol_kind KIND = PROTO_TLM;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<gpio_payload> {
    static constexpr protocol_kind KIND = PROTO_GPIO;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<clk_payload> {
    static constexpr protocol_kind KIND = PROTO_CLK;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
};

template <>
struct protocol<pci_payload> {
    static constexpr protocol_kind KIND = PROTO_PCI;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<i2c_payload> {
    static constexpr protocol_kind KIND = PROTO_I2C;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<spi_payload> {
    static constexpr protocol_kind KIND = PROTO_SPI;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<sd_command> {
    static constexpr protocol_kind KIND = PROTO_SD;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<sd_data> {
    static constexpr protocol_kind KIND = PROTO_SD;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<serial_payload> {
    static constexpr protocol_kind KIND = PROTO_SERIAL;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
};

template <>
struct protocol<vq_message> {
    static constexpr protocol_kind KIND = PROTO_VIRTIO;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

template <>
struct protocol<eth_frame> {
    static constexpr protocol_kind KIND = PROTO_ETHERNET;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
};

template <>
struct protocol<can_frame> {
    static constexpr protocol_kind KIND = PROTO_CAN;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = false;
};

template <>
struct protocol<usb_packet> {
    static constexpr protocol_kind KIND = PROTO_USB;
    static constexpr bool TRACE_FW = true;
    static constexpr bool TRACE_BW = true;
};

class tracer
{
private:
    mutable mutex m_mtx;

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

        static constexpr trace_direction translate(trace_direction dir) {
            switch (dir) {
            case TRACE_FW:
                if (!protocol<PAYLOAD>::TRACE_BW)
                    dir = TRACE_FW_NOINDENT;
                // no break
            case TRACE_FW_NOINDENT:
                return protocol<PAYLOAD>::TRACE_FW ? dir : TRACE_NONE;

            case TRACE_BW:
                if (!protocol<PAYLOAD>::TRACE_FW)
                    dir = TRACE_BW_NOINDENT;
                // no break
            case TRACE_BW_NOINDENT:
                return protocol<PAYLOAD>::TRACE_BW ? dir : TRACE_NONE;

            default:
                return TRACE_NONE;
            }
        }
    };

    virtual void trace(const activity<tlm_generic_payload>&) = 0;
    virtual void trace(const activity<gpio_payload>&) = 0;
    virtual void trace(const activity<clk_payload>&) = 0;
    virtual void trace(const activity<pci_payload>&) = 0;
    virtual void trace(const activity<i2c_payload>&) = 0;
    virtual void trace(const activity<spi_payload>&) = 0;
    virtual void trace(const activity<sd_command>&) = 0;
    virtual void trace(const activity<sd_data>&) = 0;
    virtual void trace(const activity<vq_message>&) = 0;
    virtual void trace(const activity<serial_payload>&) = 0;
    virtual void trace(const activity<eth_frame>&) = 0;
    virtual void trace(const activity<can_frame>&) = 0;
    virtual void trace(const activity<usb_packet>&) = 0;

    tracer();
    virtual ~tracer();

    template <typename PAYLOAD>
    void do_trace(const activity<PAYLOAD>& msg) {
        lock_guard<mutex> guard(m_mtx);
        trace(msg);
    }

    template <typename PAYLOAD>
    static void record(trace_direction dir, const sc_object& port,
                       const PAYLOAD& payload,
                       const sc_time& t = SC_ZERO_TIME) {
        auto& tracers = tracer::all();
        dir = activity<PAYLOAD>::translate(dir);
        if (!tracers.empty() && dir != TRACE_NONE) {
            const activity<PAYLOAD> msg = {
                protocol<PAYLOAD>::KIND,
                dir,
                failed(payload),
                port,
                payload,
                t + sc_time_stamp(),
                sc_delta_count(),
            };

            for (tracer* tr : tracers)
                tr->do_trace(msg);
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
