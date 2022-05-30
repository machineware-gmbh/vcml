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

#include "vcml/tracing/tracer_file.h"

namespace vcml {

template <typename PAYLOAD>
void tracer_file::do_trace(const activity<PAYLOAD>& msg) {
    vector<string> lines = split(to_string(msg.payload), '\n');
    for (const string& line : lines) {
        m_stream << "[" << protocol_name(msg.kind);
        print_timing(m_stream, msg);
        m_stream << "] " << msg.port.name();

        if (is_forward_trace(msg.dir))
            m_stream << " >> ";

        if (is_backward_trace(msg.dir))
            m_stream << " << ";

        m_stream << line << std::endl;
    }
}

void tracer_file::trace(const activity<tlm_generic_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<irq_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<rst_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<clk_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<pci_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<i2c_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<spi_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<sd_command>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<sd_data>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<vq_message>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<serial_payload>& msg) {
    do_trace(msg);
}

void tracer_file::trace(const activity<eth_frame>& msg) {
    do_trace(msg);
}

tracer_file::tracer_file(const string& file):
    tracer(), m_filename(file), m_stream(m_filename.c_str()) {
    VCML_ERROR_ON(!m_stream.is_open(), "failed to open %s", file.c_str());
}

tracer_file::~tracer_file() {
    // nothing to do
}

} // namespace vcml
