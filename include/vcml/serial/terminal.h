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

#ifndef VCML_SERIAL_TERMINAL_H
#define VCML_SERIAL_TERMINAL_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"
#include "vcml/common/systemc.h"

#include "vcml/serial/backend.h"
#include "vcml/protocols/serial.h"
#include "vcml/properties/property.h"

#include "vcml/module.h"

namespace vcml {
namespace serial {

class terminal : public module, public serial_host
{
private:
    struct history {
        array<u8, 4096> data;
        size_t count;
        size_t wrptr;

        history(): data(), count(), wrptr() { clear(); }

        void insert(u8 val);
        void fetch(vector<u8>& hist) const;
        void clear() { count = wrptr = 0; }
    };

    history m_hist;
    id_t m_next_id;
    unordered_map<id_t, backend*> m_backends;
    vector<backend*> m_listeners;

    bool cmd_create_backend(const vector<string>& args, ostream& os);
    bool cmd_destroy_backend(const vector<string>& args, ostream& os);
    bool cmd_list_backends(const vector<string>& args, ostream& os);
    bool cmd_history(const vector<string>& args, ostream& os);

    void serial_transmit();

    virtual void serial_receive(u8 data) override;

    static unordered_map<string, terminal*>& terminals();

public:
    property<string> backends;
    property<string> config;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    terminal(const sc_module_name& nm);
    virtual ~terminal();
    VCML_KIND(serial::terminal);

    void attach(backend* b);
    void detach(backend* b);

    id_t create_backend(const string& type);
    bool destroy_backend(id_t id);

    void fetch_history(vector<u8>& hist) { m_hist.fetch(hist); }
    void clear_history() { m_hist.clear(); }

    static terminal* find(const string& name);
    static vector<terminal*> all();

    template <typename T>
    void connect(T& device) {
        serial_tx.bind(device.serial_rx);
        device.serial_tx.bind(serial_rx);
    }
};

inline void terminal::history::insert(u8 val) {
    data[wrptr] = val;

    wrptr = (wrptr + 1) % data.size();
    count = min(count + 1, data.size());
}

inline void terminal::history::fetch(vector<u8>& hist) const {
    hist.resize(count);
    size_t pos = count == data.size() ? wrptr : 0;
    for (size_t i = 0; i < count; i++, pos = (pos + 1) % data.size())
        hist[i] = data[pos];
}

} // namespace serial
} // namespace vcml

#endif
