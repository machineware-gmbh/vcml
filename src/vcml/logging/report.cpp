/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/report.h"
#include "vcml/core/setup.h"

namespace vcml {
namespace publishers {

report::report(): publisher(LOG_ERROR, LOG_DEBUG) {
    if (vcml::setup::instance())
        VCML_ERROR("cannot use report publisher in vcml::main");
}

report::~report() {
    // nothing to do
}

void report::publish(const mwr::logmsg& msg) {
    sc_core::sc_severity severity = sc_core::SC_INFO;
    switch (msg.level) {
    case mwr::LOG_ERROR:
        severity = sc_core::SC_ERROR;
        break;
    case mwr::LOG_WARN:
        severity = sc_core::SC_WARNING;
        break;
    case mwr::LOG_INFO:
    case mwr::LOG_DEBUG:
    default:
        severity = sc_core::SC_INFO;
        break;
    }

    string text = mwr::join(msg.lines, "\n");
    sc_core::sc_report_handler::report(severity, msg.sender.c_str(),
                                       text.c_str(), msg.source.file,
                                       max(0, msg.source.line));
}

} // namespace publishers
} // namespace vcml
