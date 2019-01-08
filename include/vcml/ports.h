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

#ifndef VCML_PORTS_H
#define VCML_PORTS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    class out_port: public sc_out<bool>
    {
    private:
        bool m_state;
        bool m_stubbed;
        sc_event m_update;
        sc_signal<bool> m_stub;

        void update();

        // Disabled
        out_port(const out_port&);

    public:
        out_port();
        out_port(const sc_module_name& name);
        virtual ~out_port();

        VCML_KIND(out_port);

        void set()                    { write(true); }
        void clear()                  { write(false); }
        bool is_set() const           { return read(); }

        out_port& operator = (bool b) { write(b); return *this; }

        bool read() const             { return m_state; }
        void write(bool set);

        bool is_stubbed() const       { return m_stubbed; }
        void stub();
    };

    template <class PORT>
    class port_list: public sc_module
    {
    private:
        std::map<unsigned int, PORT*> m_ports;

        // disabled
        port_list();
        port_list(const port_list<PORT>&);

    public:
        typedef typename std::map<unsigned int, PORT*> map_type;

        typedef typename map_type::iterator iterator;
        typedef typename map_type::const_iterator const_iterator;

        port_list(const sc_module_name& nm);
        virtual ~port_list();

        VCML_KIND(port_list);

        iterator begin() { return m_ports.begin(); }
        iterator end()   { return m_ports.end(); }

        const_iterator begin() const { return m_ports.begin(); }
        const_iterator end()   const { return m_ports.end(); }

        bool exists(unsigned int idx) const ;
        PORT& operator [] (unsigned int idx);
        const PORT& operator [] (unsigned int idx) const;
    };

    template <class PORT>
    port_list<PORT>::port_list(const sc_module_name& nm):
        sc_module(nm),
        m_ports() {
        /* nothing to do */
    }

    template <class PORT>
    port_list<PORT>::~port_list() {
        for (iterator it = m_ports.begin(); it != m_ports.end(); it++)
            delete it->second;
    }

    template <class PORT>
    bool port_list<PORT>:: exists(unsigned int idx) const {
        return m_ports.find(idx) != m_ports.end();
    }

    template <class PORT>
    PORT& port_list<PORT>::operator [] (unsigned int idx) {
        if (exists(idx))
            return *m_ports[idx];

        stringstream ss;
        ss << "PORT" << idx;
        sc_core::sc_get_curr_simcontext()->hierarchy_push(this);
        PORT* port = new PORT(ss.str().c_str());
        sc_core::sc_get_curr_simcontext()->hierarchy_pop();
        m_ports[idx] = port;
        return *port;
    }

    template <class PORT>
    const PORT& port_list<PORT>::operator [] (unsigned int idx) const {
        VCML_ERROR_ON(!exists(idx), "PORT%u does not exist", idx);
        return *m_ports.at(idx);
    }

    typedef port_list<out_port>    out_port_list;
    typedef port_list<sc_in<bool>> in_port_list;

}

#endif
