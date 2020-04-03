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

#ifndef VCML_STUBS_H
#define VCML_STUBS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    class initiator_stub: public sc_module
    {
    private:
        initiator_stub();
        initiator_stub(const initiator_stub&);

        void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end);

    public:
        simple_initiator_socket<initiator_stub> OUT;

        initiator_stub(const sc_module_name&);
        virtual ~initiator_stub();
        VCML_KIND(initiator_stub);
    };

    class target_stub: public sc_module
    {
    private:
        target_stub();
        target_stub(const target_stub&);

        void b_transport(tlm_generic_payload&, sc_time&);
        unsigned int transport_dbg(tlm_generic_payload&);

    public:
        simple_target_socket<target_stub> IN;

        target_stub(const sc_module_name&);
        virtual ~target_stub();
        VCML_KIND(target_stub);
    };

}

#endif
