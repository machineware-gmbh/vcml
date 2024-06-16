/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/setup.h"
#include "vcml/core/model.h"

#include <locale.h>

namespace vcml {

static bool exit_usage() {
    std::cerr << "Usage: " << mwr::filename(mwr::progname()) << " <arguments>"
              << std::endl;
    mwr::options::print_help(std::cerr);
    exit(EXIT_FAILURE);
}

static bool exit_version() {
    std::cerr << "Modules of " << mwr::filename(mwr::progname()) << ":"
              << std::endl;
    mwr::modules::print_versions(std::cerr);
    exit(EXIT_FAILURE);
}

static bool exit_license() {
    std::cerr << "Modules of " << mwr::filename(mwr::progname()) << ":"
              << std::endl;
    mwr::modules::print_licenses(std::cerr);
    exit(EXIT_FAILURE);
}

static bool exit_models() {
    std::cerr << "Models of " << mwr::filename(mwr::progname()) << ":"
              << std::endl;
    model::list_models(std::cerr);
    exit(EXIT_FAILURE);
}

setup* setup::s_instance = nullptr;

setup::setup(int argc, char** argv):
    m_log_debug("--log-debug", "Activate verbose debug logging"),
    m_log_stdout("--log-stdout", "Send log output to stdout"),
    m_log_inscight("--log-inscight", "Send log output to InSCight database"),
    m_log_files("--log-file", "-l", "Send log output to file"),
    m_trace_stdout("--trace-stdout", "Send tracing output to stdout"),
    m_trace_inscight("--trace-inscight", "Send tracing output to InSCight"),
    m_trace_files("--trace", "-t", "Send tracing output to file"),
    m_config_files("--file", "-f", "Load configuration from file"),
    m_config_options("--config", "-c", "Specify individual property values"),
    m_help("--help", "-h", "Prints this message", exit_usage),
    m_version("--version", "Prints module version information", exit_version),
    m_license("--license", "Prints module license information", exit_license),
    m_models("--list-models", "Prints all supported models", exit_models),
    m_publishers(),
    m_brokers() {
    VCML_ERROR_ON(s_instance != nullptr, "setup already created");
    s_instance = this;

    if (!mwr::options::parse(argc, argv))
        exit_usage();

#ifdef VCML_DEBUG
    log_level min = LOG_ERROR;
    log_level max = LOG_DEBUG;
#else
    log_level min = LOG_ERROR;
    log_level max = LOG_INFO;
#endif

    if (m_log_debug.has_value())
        max = m_log_debug ? LOG_DEBUG : LOG_INFO;

    for (const string& file : m_log_files.values()) {
        mwr::publisher* pub = new mwr::publishers::file(file);
        pub->set_level(min, max);
        m_publishers.push_back(pub);
    }

    if (m_log_stdout.value() || !m_log_files.has_value()) {
        mwr::publisher* pub = new mwr::publishers::terminal(true);
        pub->set_level(min, max);
        m_publishers.push_back(pub);
    }

    if (m_log_inscight.value()) {
        mwr::publisher* pub = new vcml::publishers::inscight();
        pub->set_level(LOG_ERROR, LOG_DEBUG);
        m_publishers.push_back(pub);
    }

    for (const string& file : m_trace_files.values()) {
        tracer* t = new tracer_file(file);
        m_tracers.push_back(t);
    }

    if (m_trace_stdout) {
        tracer* t = new tracer_term(true);
        m_tracers.push_back(t);
    }

    if (m_trace_inscight) {
        tracer* t = new tracer_inscight();
        m_tracers.push_back(t);
    }

    if (m_config_options.has_value())
        m_brokers.push_back(new broker_arg(argc, argv));

    m_brokers.push_back(new broker_env());

    try {
        for (const string& file : m_config_files.values()) {
            if (mwr::ends_with(file, ".lua"))
                m_brokers.push_back(new broker_lua(file));
            else
                m_brokers.push_back(new broker_file(file));
        }
    } catch (std::exception& ex) {
        log.error(ex);
        exit(EXIT_FAILURE);
    }
}

setup::~setup() {
    s_instance = nullptr;

    for (auto broker : m_brokers)
        delete broker;

    for (auto tracer : m_tracers)
        delete tracer;

    for (auto pub : m_publishers)
        delete pub;
}

setup* setup::instance() {
    return s_instance;
}

int main(int argc, char** argv) {
#ifndef VCML_DEBUG
    // disable deprecated warning for release builds
    sc_core::sc_report_handler::set_actions("/IEEE_Std_1666/deprecated",
                                            sc_core::SC_DO_NOTHING);
#endif

    setup env(argc, argv);

    int res = EXIT_FAILURE;

    try {
        res = sc_core::sc_elab_and_sim(argc, argv);
    } catch (vcml::report& rep) {
        log.error(rep);
    } catch (std::exception& ex) {
        log.error(ex);
    }

    // at this point sc_is_running is false and no new critical sections
    // should be entered, but we need to give those who are still waiting
    // to execute a final chance to run
    thctl_flush();

    return res;
}

} // namespace vcml

extern "C" {

#ifndef MWR_MSVC
MWR_DECL_WEAK
#endif
int main(int argc, char** argv) {
    return vcml::main(argc, argv);
}
}
