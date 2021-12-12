/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_SERIAL_PORT_H
#define VCML_SERIAL_PORT_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"
#include "vcml/common/systemc.h"

#include "vcml/properties/property.h"
#include "vcml/serial/backend.h"

namespace vcml { namespace serial {

    class port
    {
    private:
        template <unsigned int N>
        struct history {
            std::array<u8, N> data;
            size_t count;
            size_t wrptr;

            history(): data(), count(0), wrptr(0) {}

            void insert(u8 val);
            void fetch(vector<u8>& hist) const;
            void clear() { count = wrptr = 0; }
        };

        string m_name;
        history<4096> m_hist;

        size_t m_next_id;
        std::map<int, backend*> m_backends;
        std::vector<backend*> m_listeners;

        bool cmd_create_backend(const vector<string>& args, ostream& os);
        bool cmd_destroy_backend(const vector<string>& args, ostream& os);
        bool cmd_list_backends(const vector<string>& args, ostream& os);
        bool cmd_history(const vector<string>& args, ostream& os);

        static unordered_map<string, port*> s_ports;

    public:
        property<string> backends;

        const char* port_name() const { return m_name.c_str(); }

        port();
        virtual ~port();

        void attach(backend* b);
        void detach(backend* b);

        int  create_backend(const string& type);
        bool destroy_backend(int id);

        void fetch_history(vector<u8>& hist);
        void clear_history();

        bool serial_in(u8& val);
        void serial_out(u8 val);

        static port* find(const string& name);
        static vector<port*> all();
    };

    template <unsigned int N>
    void port::history<N>::insert(u8 val) {
        data[wrptr] = val;
        wrptr = (wrptr + 1) % data.size();
        count = min(count + 1, data.size());
    }

    template <unsigned int N>
    void port::history<N>::fetch(vector<u8>& hist) const {
        hist.clear();
        hist.resize(count);
        size_t pos = count == data.size() ? wrptr : 0;
        for (size_t i = 0; i < count; i++) {
            hist[i] = data[pos];
            pos = (pos + 1) % data.size();
        }
    }

    inline void port::fetch_history(vector<u8>& hist) {
        m_hist.fetch(hist);
    }

    inline void port::clear_history() {
        m_hist.clear();
    }

}}

#endif
