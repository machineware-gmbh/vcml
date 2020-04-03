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

#include <signal.h>
#include <string.h>
#include <stdio.h>

#include "vcml/common/report.h"
#include "vcml/common/utils.h"
#include "vcml/common/thctl.h"
#include "vcml/common/systemc.h"

namespace vcml {

    unsigned int report::max_backtrace_length = 16;

    report::report(const string& msg, const char* file, int line):
        std::exception(),
        m_message(msg),
        m_origin(call_origin()),
        m_time(sc_time_stamp().to_seconds()),
        m_file(file),
        m_line(line),
        m_backtrace(vcml::backtrace(max_backtrace_length, 2)) {
        // nothing to do
    }

    report::~report() throw() {
        // nothing to do
    }

    const char* report::what() const throw() {
        return m_message.c_str();
    }

    static void handle_segfault(int sig, siginfo_t* info, void* context) {
        fprintf(stderr, "Backtrace\n");
        auto symbols = vcml::backtrace(report::max_backtrace_length, 2);
        for (unsigned int i = symbols.size() - 1; i < symbols.size(); i--)
            fprintf(stderr, "# %2u: %s\n", i, symbols[i].c_str());
        fprintf(stderr,
                "Caught signal %d (%s) while accessing memory at location %p\n",
                sig, strsignal(sig), info->si_addr);
        signal(sig, SIG_DFL);
        raise(sig);
    }

    void report::report_segfaults() {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);

        sa.sa_sigaction = &handle_segfault;
        sa.sa_flags = SA_NODEFER;

        if (sigaction(SIGSEGV, &sa, NULL) < 0)
            VCML_ERROR("failed to install SIGSEGV signal handler");
    }

}
