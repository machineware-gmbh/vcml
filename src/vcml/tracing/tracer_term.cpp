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

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/irq.h"
#include "vcml/protocols/rst.h"
#include "vcml/protocols/clk.h"
#include "vcml/protocols/sd.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/i2c.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/eth.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/virtio.h"

#include "vcml/tracing/tracer_term.h"

namespace vcml {

size_t tracer_term::trace_name_length = 16;
size_t tracer_term::trace_indent_incr = 1;
size_t tracer_term::trace_curr_indent = 0;

array<const char*, NUM_PROTOCOLS> tracer_term::colors = {
    /* [PROTO_TLM]      = */ termcolors::MAGENTA,
    /* [PROTO_IRQ]      = */ termcolors::YELLOW,
    /* [PROTO_RST]      = */ termcolors::RED,
    /* [PROTO_RST]      = */ termcolors::BLUE,
    /* [PROTO_PCI]      = */ termcolors::CYAN,
    /* [PROTO_I2C]      = */ termcolors::BRIGHT_GREEN,
    /* [PROTO_SPI]      = */ termcolors::BRIGHT_YELLOW,
    /* [PROTO_SD]       = */ termcolors::BRIGHT_MAGENTA,
    /* [PROTO_SERIAL]   = */ termcolors::BRIGHT_RED,
    /* [PROTO_VIRTIO]   = */ termcolors::BRIGHT_CYAN,
    /* [PROTO_ETHERNET] = */ termcolors::BRIGHT_BLUE,
};

template <typename PAYLOAD>
void tracer_term::do_trace(const activity<PAYLOAD>& msg) {
    if (m_colors)
        m_os << colors[msg.kind];

    string sender = msg.port.name();
    if (trace_name_length < sender.length())
        trace_name_length = sender.length();

    if (!msg.error && msg.dir == TRACE_FW)
        trace_curr_indent += trace_indent_incr;

    vector<string> lines = split(to_string(msg.payload), '\n');
    for (const string& line : lines) {
        m_os << "[" << protocol_name(msg.kind);
        print_timing(m_os, msg.t, msg.cycle);
        m_os << "] " << sender;

        if (trace_name_length > sender.length())
            m_os << string(trace_name_length - sender.length(), ' ');

        m_os << string(trace_curr_indent, ' ');

        if (is_forward_trace(msg.dir))
            m_os << ">> ";

        if (is_backward_trace(msg.dir))
            m_os << "<< ";

        m_os << line << std::endl;
    }

    if (m_colors)
        m_os << termcolors::CLEAR;

    if (msg.dir == TRACE_BW && !msg.error) {
        if (trace_curr_indent >= trace_indent_incr)
            trace_curr_indent -= trace_indent_incr;
        else
            trace_curr_indent = 0;
    }
}

void tracer_term::trace(const activity<tlm_generic_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<irq_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<rst_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<clk_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<pci_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<i2c_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<spi_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<sd_command>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<sd_data>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<vq_message>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<serial_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<eth_frame>& msg) {
    do_trace(msg);
}

tracer_term::tracer_term(bool use_cerr, bool use_colors):
    tracer(), m_colors(use_colors), m_os(use_cerr ? std::cerr : std::cout) {
}

tracer_term::~tracer_term() {
    // nothing to do
}

} // namespace vcml
