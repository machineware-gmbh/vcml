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

#include "vcml/setup.h"

#define PRINT(...) fprintf(stderr, ##__VA_ARGS__)

namespace vcml {

    static void exit_usage(const char* arg0, int code) {
        PRINT("Usage: %s [program arguments]\n", arg0);
        PRINT("  -l | --log <file>         Send log output to <file>\n");
        PRINT("       --log-debug          Enable debug logging\n");
        PRINT("       --no-log             Disable logging to stderr\n");
        PRINT("  -t | --trace <file>       Enable tracing to <file>\n");
        PRINT("  -f | --config-file <file> Read configuration from <file>\n");
        PRINT("  -c | --config  <x>=<y>    Set property <x> to value <y>\n");
        PRINT("  -h | --help               Prints this message\n");
        exit(code);
    }

    bool setup::parse_command_line(int argc, char** argv) {
        for (int i = 1; i < argc; i++) {
            const char* arg = argv[i];
            m_args.push_back(arg);

            if (!strcmp(arg, "--help") | !strcmp(arg, "-h")) {
                exit_usage(argv[0], EXIT_SUCCESS);

            } else if (!strcmp(arg, "--no-log")) {
                m_no_log = !m_no_log;

            } else if (!strcmp(arg, "--log-debug")) {
                m_log_debug = !m_log_debug;

            } else if (!strcmp(arg, "--log") || !strcmp(arg, "-l")) {
                if (i >= argc - 1 || *argv[i+1] == '-') {
                    PRINT("Error: %s expects <file> argument\n", arg);
                    return false;
                }

                m_log_files.push_back(argv[++i]);

            } else if (!strcmp(arg, "--trace") || !strcmp(arg, "-t")) {
                if (i >= argc - 1 || *argv[i+1] == '-') {
                    PRINT("Error: %s expects <file> argument\n", arg);
                    return false;
                }

                m_trace_files.push_back(argv[++i]);

            } else if (!strcmp(arg, "--config-file") || !strcmp(arg, "-f")) {
                if (i >= argc - 1 || *argv[i+1] == '-') {
                    PRINT("Error: %s expects <file> argument\n", arg);
                    return false;
                }

                if (!file_exists(argv[i+1])) {
                    PRINT("Error: config file not found '%s'\n", argv[i+1]);
                    return false;
                }

                m_config_files.push_back(argv[++i]);
            }
        }

        return true;
    }

    setup* setup::s_instance = nullptr;

    setup::setup(int argc, char** argv):
        m_no_log(false),
        m_log_debug(false),
        m_log_files(),
        m_trace_files(),
        m_config_files(),
        m_loggers(),
        m_providers() {
        VCML_ERROR_ON(s_instance != nullptr, "setup already created");
        s_instance = this;

        if (!parse_command_line(argc, argv))
            exit_usage(argv[0], EXIT_FAILURE);

        log_level min = LOG_ERROR;
        log_level max = m_log_debug ? LOG_DEBUG : LOG_INFO;

        if (!m_no_log) {
            logger* log = new log_term(true);
            log->set_level(min, max);
            m_loggers.push_back(log);
        }

        for (string file : m_log_files) {
            logger* log = new log_file(file);
            log->set_level(min, max);
            m_loggers.push_back(log);
        }

        for (string file : m_trace_files) {
            logger* log = new log_file(file);
            log->set_level(LOG_TRACE, LOG_TRACE);
            m_loggers.push_back(log);
        }

        m_providers.push_back(new property_provider_arg(argc, argv));
        m_providers.push_back(new property_provider_env());

        for (string file : m_config_files)
            m_providers.push_back(new property_provider_file(file));
    }

    setup::~setup() {
        for (auto provider : m_providers)
            delete provider;
        for (auto logger : m_loggers)
            delete logger;
    }

    setup& setup::instance() {
        VCML_ERROR_ON(s_instance == nullptr, "setup unavailable");
        return *s_instance;
    }

    int main(int argc, char** argv) {
        setup s(argc, argv);

#ifdef NDEBUG
        // disable deprecated warning for release builds
        sc_core::sc_report_handler::set_actions("/IEEE_Std_1666/deprecated",
                                                sc_core::SC_DO_NOTHING);
#endif

        try {
            return sc_core::sc_elab_and_sim(argc, argv);
        } catch (vcml::report& r) {
            vcml::logger::log(r);
            return EXIT_FAILURE;
        } catch (std::exception& e) {
            vcml::log_error(e.what());
            return EXIT_FAILURE;
        }
    }

}

extern "C" int main(int argc, char** argv) {
    return vcml::main(argc, argv);
}
