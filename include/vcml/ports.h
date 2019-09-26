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

    template <typename T>
    class out_port: public sc_out<T>
    {
    private:
        T m_state;
        sc_event m_update;
        sc_signal<T>* m_stub;

        void update();

    public:
        out_port();
        out_port(const sc_module_name& name);
        virtual ~out_port();
        VCML_KIND(out_port);

        out_port(const out_port<T>& copy) = delete;
        out_port(out_port<T>&& other) = delete;

        out_port<T>& operator = (T val);

        const T& read() const;
        void write(T val);

        bool is_stubbed() const;
        void stub();
    };

    template <typename T>
    void out_port<T>::update() {
        if ((*this)->read() != m_state)
            (*this)->write(m_state);
    }

    template <typename T>
    out_port<T>::out_port():
        sc_core::sc_out<T>(sc_gen_unique_name("out")),
        m_state(),
        m_update(concat(this->basename(), "_update_ev").c_str()),
        m_stub(nullptr) {
        sc_core::sc_spawn_options opts;
        opts.spawn_method();
        opts.set_sensitivity(&m_update);
        opts.dont_initialize();

        sc_spawn(sc_bind(&out_port<T>::update, this),
                 concat(this->basename(), "_update").c_str(),
                  &opts);
    }

    template <typename T>
    out_port<T>::out_port(const sc_module_name& nm):
        sc_core::sc_out<T>(nm),
        m_state(false),
        m_update(concat(this->basename(), "_update_ev").c_str()),
        m_stub(nullptr) {
        sc_core::sc_spawn_options opts;
        opts.spawn_method();
        opts.set_sensitivity(&m_update);
        opts.dont_initialize();

        sc_spawn(sc_bind(&out_port::update, this),
                 concat(this->basename(), "_update").c_str(),
                 &opts);
    }

    template <typename T>
    out_port<T>::~out_port() {
        if (m_stub != nullptr)
            delete m_stub;
    }

    template <typename T>
    const T& out_port<T>::read() const {
        return m_state;
    }

    template <typename T>
    out_port<T>& out_port<T>::operator = (T val) {
        write(val);
        return *this;
    }

    template <typename T>
    void out_port<T>::write(T val) {
        m_state = val;

        sc_simcontext* simc = sc_object::simcontext();

        // Only notify the updater process if necessary, i.e. when the signal
        // value has to change. However, if the port has not been bound, we
        // always notify since we cannot know the state of the future signal.
        if (sc_out<T>::bind_count() == 0 || (*this)->read() != val) {
            if (!simc->evaluation_phase())
                m_update.notify(SC_ZERO_TIME);
            else
                m_update.notify();
        }
    }

    template <typename T>
    bool out_port<T>::is_stubbed() const {
        return m_stub != nullptr;
    }

    template <typename T>
    void out_port<T>::stub() {
        if (is_stubbed())
            return;

        sc_simcontext* simc = sc_object::simcontext();
        sc_module* p = dynamic_cast<sc_module*>(sc_object::get_parent_object());
        VCML_ERROR_ON(!p, "failed to lookup parent module");

        simc->hierarchy_push(p);
        m_stub = new sc_signal<T>(concat(this->basename(), "_stub").c_str());
        bind(*m_stub);
        simc->hierarchy_pop();
    }

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

    template <typename T>
    using out_port_list = port_list<out_port<T>>;

    template <typename T>
    using in_port_list = port_list<sc_in<T>>;

}

#endif
