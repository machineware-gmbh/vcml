/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/clk.h"
#include "vcml/protocols/sd.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/i2c.h"
#include "vcml/protocols/lin.h"
#include "vcml/protocols/pci.h"
#include "vcml/protocols/eth.h"
#include "vcml/protocols/can.h"
#include "vcml/protocols/usb.h"
#include "vcml/protocols/serial.h"
#include "vcml/protocols/signal.h"
#include "vcml/protocols/virtio.h"

#include "vcml/tracing/tracer_file.h"

namespace vcml {

void tracer_file::trace(const trace_activity& msg) {
    vector<string> lines = split(escape(msg.to_string()), '\n');
    for (const string& line : lines) {
        m_stream << "[" << msg.protocol_name();
        print_timing(m_stream, msg);
        m_stream << "] " << msg.port.name();

        if (is_forward_trace(msg.dir))
            m_stream << " >> ";

        if (is_backward_trace(msg.dir))
            m_stream << " << ";

        m_stream << line << std::endl;
    }
}

tracer_file::tracer_file(const string& file):
    tracer(), m_filename(file), m_stream(m_filename.c_str()) {
    VCML_ERROR_ON(!m_stream.is_open(), "failed to open %s", file.c_str());
}

tracer_file::~tracer_file() {
    // nothing to do
}

} // namespace vcml
