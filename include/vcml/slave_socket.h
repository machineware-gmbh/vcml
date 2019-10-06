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

#ifndef VCML_SLAVE_SOCKET_H
#define VCML_SLAVE_SOCKET_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/sbi.h"
#include "vcml/exmon.h"
#include "vcml/dmi_cache.h"
#include "vcml/component.h"

namespace vcml {

    class slave_socket: public simple_target_socket<slave_socket, 64>
    {
    private:
        bool       m_free;
        sc_event   m_free_ev;
        dmi_cache  m_dmi_cache;
        exmon      m_exmon;
        component* m_host;

        void b_transport(tlm_generic_payload& tx, sc_time& dt);
        unsigned int transport_dbg(tlm_generic_payload& tx);
        bool get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi);

    public:
        slave_socket(const char* name, component* host = NULL);
        virtual ~slave_socket();

        VCML_KIND(slave_socket);

        dmi_cache& dmi()   { return m_dmi_cache; }
        exmon&     exmem() { return m_exmon; }

        void map_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);
        void remap_dmi(const sc_time& rlat, const sc_time& wlat);
        void invalidate_dmi();
    };

    inline void slave_socket::map_dmi(const tlm_dmi& dmi) {
        m_dmi_cache.insert(dmi);
    }

}

#endif
