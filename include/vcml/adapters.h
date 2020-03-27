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

#ifndef VCML_ADAPTERS_H
#define VCML_ADAPTERS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    template <unsigned int WIDTH_IN, unsigned int WIDTH_OUT>
    class bus_width_adapter: public sc_module
    {
    public:
        typedef bus_width_adapter<WIDTH_IN, WIDTH_OUT> this_type;
        tlm_utils::simple_target_socket<bus_width_adapter, WIDTH_IN> IN;
        tlm_utils::simple_initiator_socket<bus_width_adapter, WIDTH_OUT> OUT;

        bus_width_adapter() = delete;

        bus_width_adapter(const sc_module_name& nm):
            sc_module(nm),
            IN("IN"),
            OUT("OUT") {
            IN.register_b_transport(this, &this_type::b_transport);
            IN.register_transport_dbg(this, &this_type::transport_dbg);
            IN.register_get_direct_mem_ptr(this,
                &this_type::get_direct_mem_ptr);
            OUT.register_invalidate_direct_mem_ptr(this,
                &this_type::invalidate_direct_mem_ptr);
        }

        virtual ~bus_width_adapter() {
            // nothing to do
        }

        VCML_KIND(bus_width_adapter);

    private:
        void b_transport(tlm_generic_payload& tx, sc_time& t) {
            OUT->b_transport(tx, t);
        }

        unsigned int transport_dbg(tlm_generic_payload& tx) {
            return OUT->transport_dbg(tx);
        }

        bool get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi) {
            return OUT->get_direct_mem_ptr(tx, dmi);
        }

        void invalidate_direct_mem_ptr(sc_dt::uint64 s, sc_dt::uint64 e) {
            IN->invalidate_direct_mem_ptr(s, e);
        }
    };

}

#endif
