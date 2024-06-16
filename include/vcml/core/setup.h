/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SETUP_H
#define VCML_SETUP_H

#include "vcml/core/types.h"
#include "vcml/core/thctl.h"

#include "vcml/logging/logger.h"
#include "vcml/logging/inscight.h"

#include "vcml/tracing/tracer.h"
#include "vcml/tracing/tracer_file.h"
#include "vcml/tracing/tracer_term.h"
#include "vcml/tracing/tracer_inscight.h"

#include "vcml/properties/property.h"
#include "vcml/properties/broker.h"
#include "vcml/properties/broker_arg.h"
#include "vcml/properties/broker_env.h"
#include "vcml/properties/broker_file.h"
#include "vcml/properties/broker_lua.h"

namespace vcml {

class setup
{
private:
    mwr::option<bool> m_log_debug;
    mwr::option<bool> m_log_stdout;
    mwr::option<bool> m_log_inscight;
    mwr::option<string> m_log_files;

    mwr::option<bool> m_trace_stdout;
    mwr::option<bool> m_trace_inscight;
    mwr::option<string> m_trace_files;

    mwr::option<string> m_config_files;
    mwr::option<string> m_config_options;

    mwr::option<bool> m_help;
    mwr::option<bool> m_version;
    mwr::option<bool> m_license;
    mwr::option<bool> m_models;

    vector<mwr::publisher*> m_publishers;
    vector<tracer*> m_tracers;
    vector<broker*> m_brokers;

    static setup* s_instance;

public:
    setup() = delete;
    setup(int argc, char** argv);
    virtual ~setup();

    bool is_logging_debug() const { return m_log_debug; }
    bool is_logging_stdout() const { return m_log_stdout; }
    bool is_tracing_stdout() const { return m_trace_stdout; }

    const vector<string>& log_files() const;
    const vector<string>& trace_files() const;
    const vector<string>& config_files() const;

    static setup* instance();
};

inline const vector<string>& setup::log_files() const {
    return m_log_files.values();
}

inline const vector<string>& setup::trace_files() const {
    return m_trace_files.values();
}

inline const vector<string>& setup::config_files() const {
    return m_config_files.values();
}

int main(int argc, char** argv);

} // namespace vcml

#endif
