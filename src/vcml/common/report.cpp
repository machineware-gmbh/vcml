/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/common/report.h"
#include "vcml/common/thctl.h"
#include "vcml/logging/logger.h"

namespace vcml {

    bool g_backtrace_on_error = true;

    static string find_source() {
        pthread_t this_thread = pthread_self();
        if (this_thread != thctl_sysc_thread()) {
            char buffer[16] = { 0 };
            pthread_getname_np(this_thread, buffer, sizeof(buffer));
            return mkstr("pthread '%s'", buffer);
        }

        sc_core::sc_simcontext* simc = sc_core::sc_get_curr_simcontext();
        if (simc) {
            sc_process_b* proc = sc_get_current_process_b();
            if (proc)
                return proc->name();

            sc_module* module = simc->hierarchy_curr();
            if (module)
                return module->name();
        }

        return "";
    }

    void report::init() {
        m_source = find_source();

        stringstream ss; ss << *this;
        m_desc = ss.str();
    }

    report::report(const string& msg):
        std::exception(),
        m_severity(SEVERITY_ERROR),
        m_message(msg),
        m_source(),
        m_time(sc_time_stamp()),
        m_file(),
        m_line(0),
        m_backtrace(backtrace(16, 2)) {
        init();
    }

    report::report(severity sev, const string& msg):
        std::exception(),
        m_severity(sev),
        m_message(msg),
        m_source(),
        m_time(sc_time_stamp()),
        m_file(),
        m_line(0),
        m_backtrace(backtrace(16, 2)) {
        init();
    }

    report::report(severity sev, const string& msg, const char* file, int ln):
        std::exception(),
        m_severity(sev),
        m_message(msg),
        m_source(),
        m_time(sc_time_stamp()),
        m_file(file),
        m_line(ln),
        m_backtrace(backtrace(16, 2)) {
        init();
    }

    report::~report() throw() {
        // nothing to do
    }

    void report::write(ostream& os) const {
        vector<string> lines = split(m_message, '\n');
        for (unsigned int i = 0; i < lines.size(); i++) {
            write_severity_and_time(os);
            if (!m_source.empty())
                os << m_source << ": ";
            os << lines[i];
            if (i < (lines.size() - 1))
                os << std::endl;
        }

        if (!m_file.empty()) {
            os << std::endl;
            write_severity_and_time(os);
            os << "(" << m_file << ":" << m_line << ")";
        }

        if ((m_severity == SEVERITY_ERROR) && g_backtrace_on_error) {
            os << std::endl;
            write_backtrace(os);
        }
    }

    void report::write_severity_and_time(ostream& os) const {
        os << "[" << vcml::report::prefix[m_severity] << " "
           << std::fixed << std::setprecision(9)
           << m_time.to_seconds() << "s] ";
    }

    void report::write_backtrace(ostream& os) const {
        size_t count = m_backtrace.size();

        write_severity_and_time(os);
        os << "Backtrace(" << count << ")" << std::endl;
        for (unsigned int i = count - 1; i < count; i--) {
            write_severity_and_time(os);
            os << "#" << i << ": " << m_backtrace[i];
            if (i != 0)
                os << std::endl;
        }
    }

    const char* report::what() const throw() {
        //return m_desc.c_str();
        return m_message.c_str();
    }

    const char* report::prefix[SEVERITY_MAX] = {
            [SEVERITY_ERROR] = "E",
            [SEVERITY_WARNING] = "W",
            [SEVERITY_INFO] = "I",
            [SEVERITY_DEBUG] = "D"
    };

    const char* report::desc[SEVERITY_MAX] = {
            [SEVERITY_ERROR] = "error",
            [SEVERITY_WARNING] = "warning",
            [SEVERITY_INFO] = "info",
            [SEVERITY_DEBUG] = "debug"
    };

    string vcml_report_compose_message(const sc_report& rep) {
        stringstream ss;
        ss << sc_core::sc_report_compose_message(rep);
        if (rep.get_severity() >= sc_core::SC_ERROR && g_backtrace_on_error) {
            // skip compose, handler and report
            const unsigned int skip_frames = 3;
            vector<string> bt = backtrace(20 + skip_frames);
            for (unsigned int i = bt.size() - 1; i >= skip_frames; i--)
                ss << std::endl << i - skip_frames << ": " << bt[i];
        }

        return ss.str();
    }

    void report_handler(const sc_report& rep, const sc_actions& actions_) {
        sc_actions actions(actions_);

        if (actions & sc_core::SC_DISPLAY) {
            std::cout << std::endl << vcml_report_compose_message(rep)
                      << std::endl;
            actions &= ~sc_core::SC_DISPLAY;
        }

        if (actions & sc_core::SC_LOG) {
            switch (rep.get_severity()) {
            case sc_core::SC_INFO:
                log_info(vcml_report_compose_message(rep).c_str());
                break;
            case sc_core::SC_WARNING:
                log_warning(vcml_report_compose_message(rep).c_str());
                break;
            default:
                log_error(vcml_report_compose_message(rep).c_str());
                break;
            }
        }

        if (rep.get_severity() >= sc_core::SC_ERROR)
            actions |= sc_core::SC_THROW;

        sc_core::sc_report_handler::default_handler(rep, actions);
    }

    void initialize_reporting() {
        sc_core::sc_report_handler::set_handler(report_handler);

        sc_core::sc_report_handler::set_actions(VCML_DOMAIN,
                sc_core::SC_FATAL, sc_core::SC_LOG | sc_core::SC_THROW);
        sc_core::sc_report_handler::set_actions(VCML_DOMAIN,
                sc_core::SC_ERROR, sc_core::SC_LOG | sc_core::SC_THROW);
        sc_core::sc_report_handler::set_actions(VCML_DOMAIN,
                sc_core::SC_WARNING, sc_core::SC_LOG);
        sc_core::sc_report_handler::set_actions(VCML_DOMAIN,
                sc_core::SC_INFO, sc_core::SC_LOG);
    }
}

std::ostream& operator << (std::ostream& os, const vcml::severity sev) {
    if (sev < vcml::SEVERITY_ERROR || sev > vcml::SEVERITY_DEBUG)
        return os << "unknown";
    return os << vcml::report::desc[sev];
}

std::istream& operator >> (std::istream& is, vcml::severity& sev) {
    std::string s; is >> s;
    for (int i = vcml::SEVERITY_ERROR; i < vcml::SEVERITY_MAX; i++)
        if (s == vcml::report::desc[i])
            sev = (vcml::severity)i;
    return is;
}

std::ostream& operator << (std::ostream& os, const vcml::report& rep) {
    rep.write(os);
    return os;
}
