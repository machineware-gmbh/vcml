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

#ifndef VCML_TRACER_FILE_H
#define VCML_TRACER_FILE_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/tracing/tracer.h"

namespace vcml {

class tracer_file : public tracer
{
private:
    string m_filename;
    ofstream m_stream;

    template <typename PAYLOAD>
    void do_trace(const activity<PAYLOAD>& msg);

public:
    const char* filename() const { return m_filename.c_str(); }

    virtual void trace(const activity<tlm_generic_payload>&) override;
    virtual void trace(const activity<irq_payload>&) override;
    virtual void trace(const activity<pci_payload>&) override;
    virtual void trace(const activity<spi_payload>&) override;
    virtual void trace(const activity<sd_command>&) override;
    virtual void trace(const activity<sd_data>&) override;
    virtual void trace(const activity<vq_message>&) override;

    tracer_file(const string& filename);
    virtual ~tracer_file();
};

} // namespace vcml

#endif
