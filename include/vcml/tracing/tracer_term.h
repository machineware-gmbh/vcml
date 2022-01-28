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

#ifndef VCML_TRACER_TERM_H
#define VCML_TRACER_TERM_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/tracing/tracer.h"

namespace vcml {

    class tracer_term : public tracer
    {
    private:
        bool m_colors;
        ostream& m_os;

        template <typename PAYLOAD>
        void do_trace(const entry<PAYLOAD>& msg);

    public:
        bool has_colors() const { return m_colors; }
        void set_colors(bool set = true) { m_colors = set; }

        virtual void trace(const entry<tlm_generic_payload>&) override;
        virtual void trace(const entry<irq_payload>&) override;
        virtual void trace(const entry<pci_payload>&) override;
        virtual void trace(const entry<spi_payload>&) override;
        virtual void trace(const entry<sd_command>&) override;
        virtual void trace(const entry<sd_data>&) override;
        virtual void trace(const entry<vq_message>&) override;

        tracer_term(bool use_cerr = false, bool use_colors = true);
        virtual ~tracer_term();

        static size_t trace_name_length;
        static size_t trace_indent_incr;
        static size_t trace_curr_indent;

        static const char* colors[NUM_PROTOCOLS];
    };

}

#endif
