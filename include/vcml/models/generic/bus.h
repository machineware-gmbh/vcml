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

#ifndef VCML_GENERIC_BUS
#define VCML_GENERIC_BUS

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/command.h"
#include "vcml/component.h"

namespace vcml { namespace generic {

    class bus;

    template <typename T>
    class bus_ports
    {
    private:
        unsigned int m_next;
        bus* m_parent;
        std::map<unsigned int, T*> m_sockets;

    public:
        bus_ports() = delete;
        bus_ports(bus* parent);
        virtual ~bus_ports();

        bool exists(unsigned int idx) const;

        unsigned int next_idx() const { return m_next; }
        T& next() { return operator [] (next_idx()); }

        T& operator[] (unsigned int idx);
        const T& operator[] (unsigned int idx) const;

        typedef typename std::map<unsigned int, T*>::iterator iterator;

        iterator begin() { return m_sockets.begin(); }
        iterator end()   { return m_sockets.end(); }
    };

    template <typename T>
    inline bus_ports<T>::bus_ports(bus* parent):
        m_next(0),
        m_parent(parent),
        m_sockets() {
        VCML_ERROR_ON(parent == nullptr, "bus_ports parent must not be NULL");
    }

    template <typename T>
    inline bus_ports<T>::~bus_ports() {
        for (auto it : m_sockets)
            delete it.second;
    }

    template <typename T>
    inline bool bus_ports<T>::exists(unsigned int idx) const {
        return m_sockets.find(idx) != m_sockets.end();
    }

    template <typename T>
    inline const T& bus_ports<T>::operator[] (unsigned int idx) const {
        VCML_ERROR_ON(!exists(idx), "bus port %d does not exist", idx);
        return *m_sockets.at(idx);
    }

    class bus: public component
    {
    public:
        typedef tlm_initiator_socket<64> initiator_socket;
        typedef tlm_target_socket<64> target_socket;

    private:
        bool cmd_mmap(const vector<string>& args, ostream& os);

        struct mapping {
            int port;
            range addr;
            u64 offset;
            string peer;
        };

        vector<mapping> m_mappings;
        mapping         m_default;

        target_socket*    create_target_socket(unsigned int idx);
        initiator_socket* create_initiator_socket(unsigned int idx);

        void cb_b_transport(int port, tlm_generic_payload& tx, sc_time& dt);
        unsigned int cb_transport_dbg(int port, tlm_generic_payload& tx);
        bool cb_get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                                   tlm_dmi& dmi);
        void cb_invalidate_direct_mem_ptr(int port, sc_dt::uint64 s,
                                          sc_dt::uint64 e);

        using component::b_transport;
        using component::transport_dbg;
        using component::get_direct_mem_ptr;
        using component::invalidate_direct_mem_ptr;

    protected:
        void b_transport(int port, tlm_generic_payload& tx, sc_time& dt);
        unsigned int transport_dbg(int port, tlm_generic_payload& tx);
        bool get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                                tlm_dmi& dmi);
        void invalidate_direct_mem_ptr(int port, sc_dt::uint64 start,
                                       sc_dt::uint64 end);

    public:
        bus_ports<target_socket> IN;
        bus_ports<initiator_socket> OUT;

        const mapping& lookup(const range& addr) const;

        void map(unsigned int port, const range& addr, u64 offset = 0,
                 const string& peer = "");
        void map(unsigned int port, u64 start, u64 end, u64 offset = 0,
                 const string& peer = "");
        void map_default(unsigned int port, u64 offset = 0,
                         const string& peer = "");

        unsigned int bind(initiator_socket& socket);
        unsigned int bind(target_socket& socket, const range& addr,
                          u64 offset = 0);
        unsigned int bind(target_socket& socket, u64 start, u64 end,
                          u64 offset = 0);
        unsigned int bind_default(initiator_socket& socket, u64 offset = 0);
        unsigned int bind_default(target_socket& socket, u64 offset = 0);

        bus() = delete;
        bus(const sc_module_name& nm);
        virtual ~bus();
        VCML_KIND(bus);

        template <typename T>
        T* create_socket(unsigned int idx);

        template <typename T>
        void trace_fw(const T& socket, const tlm_generic_payload& tx,
                      const sc_time& dt) const;

        template <typename T>
        void trace_bw(const T& socket, const tlm_generic_payload& tx,
                      const sc_time& dt) const;
    };

    template <typename T>
    inline void bus::trace_fw(const T& s, const tlm_generic_payload& tx,
                              const sc_time& dt) const {
        if (loglvl >= LOG_TRACE && !trace_errors)
            logger::trace_fw(s.name(), tx, dt);
    }

    template <typename T>
    inline void bus::trace_bw(const T& s, const tlm_generic_payload& tx,
                              const sc_time& dt) const {
        if (loglvl >= LOG_TRACE && (!trace_errors || failed(tx)))
            logger::trace_bw(s.name(), tx, dt);
    }

    template <typename T>
    inline T& bus_ports<T>::operator[] (unsigned int idx) {
        if (!exists(idx)) {
            m_sockets[idx] = m_parent->create_socket<T>(idx);
            m_next = idx + 1;
        }

        return *m_sockets[idx];
    }

}}

#endif
