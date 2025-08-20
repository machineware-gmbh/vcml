/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACING_TRACER_TERM_H
#define VCML_TRACING_TRACER_TERM_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/tracing/protocol.h"
#include "vcml/tracing/activity.h"
#include "vcml/tracing/tracer.h"

namespace vcml {

class tracer_term : public tracer
{
private:
    bool m_colors;
    ostream& m_os;

public:
    bool has_colors() const { return m_colors; }
    void set_colors(bool set = true) { m_colors = set; }

    virtual void trace(const trace_activity&) override;

    tracer_term(bool use_cerr = false);
    tracer_term(bool use_cerr, bool use_colors);
    virtual ~tracer_term();

    static size_t trace_name_length;
    static size_t trace_indent_incr;
    static size_t trace_curr_indent;
};

} // namespace vcml

#endif
