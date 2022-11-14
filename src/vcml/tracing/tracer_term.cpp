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
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/clk.h"
#include "vcml/protocols/sd.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/i2c.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/eth.h"
#include "vcml/protocols/can.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/virtio.h"

#include "vcml/tracing/tracer_term.h"

namespace vcml {

size_t tracer_term::trace_name_length = 16;
size_t tracer_term::trace_indent_incr = 1;
size_t tracer_term::trace_curr_indent = 0;

array<const char*, NUM_PROTOCOLS> tracer_term::colors = {
    /* [PROTO_TLM]      = */ mwr::termcolors::MAGENTA,
    /* [PROTO_GPIO]     = */ mwr::termcolors::YELLOW,
    /* [PROTO_CLK]      = */ mwr::termcolors::BLUE,
    /* [PROTO_PCI]      = */ mwr::termcolors::CYAN,
    /* [PROTO_I2C]      = */ mwr::termcolors::BRIGHT_GREEN,
    /* [PROTO_SPI]      = */ mwr::termcolors::BRIGHT_YELLOW,
    /* [PROTO_SD]       = */ mwr::termcolors::BRIGHT_MAGENTA,
    /* [PROTO_SERIAL]   = */ mwr::termcolors::BRIGHT_RED,
    /* [PROTO_VIRTIO]   = */ mwr::termcolors::BRIGHT_CYAN,
    /* [PROTO_ETHERNET] = */ mwr::termcolors::BRIGHT_BLUE,
    /* [PROTO_CAN]      = */ mwr::termcolors::RED,
};

template <typename PAYLOAD>
void tracer_term::do_trace(const activity<PAYLOAD>& msg) {
    VCML_ERROR_ON(!m_os.good(), "trace stream broken");

    stringstream ss;
    if (m_colors)
        ss << colors[msg.kind];

    string sender = msg.port.name();
    if (trace_name_length < sender.length())
        trace_name_length = sender.length();

    if (msg.dir == TRACE_FW)
        trace_curr_indent += trace_indent_incr;

    vector<string> lines = split(to_string(msg.payload), '\n');
    for (const string& line : lines) {
        ss << "[" << protocol_name(msg.kind);
        print_timing(ss, msg.t, msg.cycle);
        ss << "] " << sender;

        if (trace_name_length > sender.length())
            ss << string(trace_name_length - sender.length(), ' ');

        ss << string(trace_curr_indent, ' ');

        if (is_forward_trace(msg.dir))
            ss << ">> ";

        if (is_backward_trace(msg.dir))
            ss << "<< ";

        ss << line << std::endl;
    }

    if (m_colors)
        ss << mwr::termcolors::CLEAR;

    m_os << ss.rdbuf() << std::flush;

    if (msg.dir == TRACE_BW && trace_curr_indent) {
        if (trace_curr_indent >= trace_indent_incr)
            trace_curr_indent -= trace_indent_incr;
        else
            trace_curr_indent = 0;
    }
}

void tracer_term::trace(const activity<tlm_generic_payload>& msg) {
    do_trace(msg);
}

void tracer_term::trace(const activity<gpio_payload>& msg) {
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

void tracer_term::trace(const activity<can_frame>& msg) {
    do_trace(msg);
}

tracer_term::tracer_term(bool use_cerr):
    tracer_term(use_cerr, isatty(use_cerr ? STDERR_FILENO : STDOUT_FILENO)) {
}

tracer_term::tracer_term(bool use_cerr, bool use_colors):
    tracer(), m_colors(use_colors), m_os(use_cerr ? std::cerr : std::cout) {
}

tracer_term::~tracer_term() {
    // nothing to do
}

} // namespace vcml
