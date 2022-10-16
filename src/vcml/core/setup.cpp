/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/core/setup.h"

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

setup* setup::s_instance = nullptr;

setup::setup(int argc, char** argv):
    m_log_debug("--log-debug", "Activate verbose debug logging"),
    m_log_delta("--log-delta", "Include delta cycles in log"),
    m_log_stdout("--log-stdout", "Send log output to stdout"),
    m_log_files("--log-file", "-l", "Send log output to file"),
    m_trace_stdout("--trace-stdout", "Send tracing output to stdout"),
    m_trace_files("--trace", "-t", "Send tracing output to file"),
    m_config_files("--file", "-f", "Load configuration from file"),
    m_config_options("--config", "-c", "Specify individual property values"),
    m_help("--help", "-h", "Prints this message", exit_usage),
    m_version("--version", "Prints module version information", exit_version),
    m_publishers(),
    m_brokers() {
    VCML_ERROR_ON(s_instance != nullptr, "setup already created");
    s_instance = this;

    if (!mwr::options::parse(argc, argv))
        exit_usage();

    log_level min = LOG_ERROR;
    log_level max = m_log_debug ? LOG_DEBUG : LOG_INFO;

    for (const string& file : m_log_files.values()) {
        publisher* pub = new log_file(file);
        pub->set_level(min, max);
        m_publishers.push_back(pub);
    }

    if (m_log_stdout.value() || !m_log_files.has_value()) {
        publisher* pub = new log_term(true);
        pub->set_level(min, max);
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

    if (m_config_options.has_value())
        m_brokers.push_back(new broker_arg(argc, argv));

    m_brokers.push_back(new broker_env());

    for (const string& file : m_config_files.values())
        m_brokers.push_back(new broker_file(file));
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
    setlocale(LC_ALL, "");

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

extern "C" int main(int argc, char** argv) MWR_DECL_WEAK;
extern "C" int main(int argc, char** argv) {
    return vcml::main(argc, argv);
}
