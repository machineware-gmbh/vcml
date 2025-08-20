/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACING_TRACER_FILE_H
#define VCML_TRACING_TRACER_FILE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/tracing/tracer.h"

namespace vcml {

class tracer_file : public tracer
{
private:
    string m_filename;
    ofstream m_stream;

    template <typename PAYLOAD>
    void do_trace(const trace_activity_proto<PAYLOAD>& msg);

public:
    const char* filename() const { return m_filename.c_str(); }

    virtual void trace(const trace_activity&) override;

    tracer_file(const string& filename);
    virtual ~tracer_file();
};

} // namespace vcml

#endif
