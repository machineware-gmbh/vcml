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

#ifndef VCML_GENERIC_CROSSBAR_H
#define VCML_GENERIC_CROSSBAR_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"


namespace vcml { namespace generic {

    class crossbar: public peripheral
    {
    private:
        mutable std::map<u64, bool> m_forward;

        u64 idx(unsigned int from, unsigned int to) const;

        void forward(unsigned int);

        // Disabled
        crossbar();
        crossbar(const crossbar&);

    public:
        in_port_list<bool> IN;
        out_port_list<bool> OUT;

        bool is_forward(unsigned int from, unsigned int to) const;
        void set_forward(unsigned int from, unsigned int to);
        void set_no_forward(unsigned int from, unsigned int to);

        bool is_broadcast(unsigned int from) const;
        void set_broadcast(unsigned int from);
        void set_no_broadcast(unsigned int from);

        crossbar(const sc_module_name& name);
        virtual ~crossbar();

        VCML_KIND(crossbar);

    protected:
        virtual void end_of_elaboration();
    };

    inline u64 crossbar::idx(unsigned int from, unsigned int to) const {
        u64 idx_hi = from & 0xffffffffull;
        u64 idx_lo = to & 0xffffffffull;
        return idx_hi << 32 | idx_lo;
    }

    inline bool crossbar::is_forward(unsigned int from, unsigned int to) const {
        return m_forward[idx(from, to)];
    }

    inline void crossbar::set_forward(unsigned int from, unsigned int to) {
        m_forward[idx(from, to)] = true;
    }

    inline void crossbar::set_no_forward(unsigned int from, unsigned int to) {
        m_forward[idx(from, to)] = false;
    }

    inline bool crossbar::is_broadcast(unsigned int from) const {
        return m_forward[idx(from, (unsigned int)-1)];
    }

    inline void crossbar::set_broadcast(unsigned int from) {
        m_forward[idx(from, (unsigned int)-1)] = true;
    }

    inline void crossbar::set_no_broadcast(unsigned int from) {
        m_forward[idx(from, (unsigned int)-1)] = false;
    }

}}

#endif
