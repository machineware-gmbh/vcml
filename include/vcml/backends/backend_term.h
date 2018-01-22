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

#ifndef VCML_BACKEND_TERM_H
#define VCML_BACKEND_TERM_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"
#include "vcml/backends/backend.h"

namespace vcml {

    class backend_term: public backend
    {
    private:
        int m_signal;
        bool m_exit;
        bool m_stopped;

        termios m_termios;
        timeval m_time;

        sighandler_t m_sigint;
        sighandler_t m_sigstp;

        static backend_term* singleton;
        static void handle_signal(int sig);

        void handle_sigstp(int sig);
        void handle_sigint(int sig);

        void cleanup();

    public:
        backend_term(const sc_module_name& name = "backend");
        virtual ~backend_term();

        VCML_KIND(backend_term);

        virtual size_t peek();
        virtual size_t read(void* buf, size_t len);
        virtual size_t write(const void* buf, size_t len);

        static backend* create(const string& name);
    };

}

#endif
