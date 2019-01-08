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

#include "vcml/ports.h"

namespace vcml {

    void out_port::update() {
        if ((*this)->read() != m_state)
            (*this)->write(m_state);
    }

    out_port::out_port():
        sc_core::sc_out<bool>(sc_gen_unique_name("out")),
        m_state(false),
        m_stubbed(false),
        m_update(),
        m_stub(concat(basename(), "_stub").c_str()) {
        sc_core::sc_spawn_options opts;
        opts.spawn_method();
        opts.set_sensitivity(&m_update);
        opts.dont_initialize();

        sc_spawn(sc_bind(&out_port::update, this),
                 concat(name(), "_update").c_str(),
                  &opts);
    }

    out_port::out_port(const sc_module_name& nm):
        sc_core::sc_out<bool>(nm),
        m_state(false),
        m_stubbed(false),
        m_update(),
        m_stub(concat(basename(), "_stub").c_str()) {
        sc_core::sc_spawn_options opts;
        opts.spawn_method();
        opts.set_sensitivity(&m_update);
        opts.dont_initialize();

        sc_spawn(sc_bind(&out_port::update, this),
                 concat(basename(), "_update").c_str(),
                 &opts);
    }

    out_port::~out_port() {
        /* nothing to do */
    }

    void out_port::write(bool set) {
        m_state = set;
        if ((*this)->read() != set)
            m_update.notify(SC_ZERO_TIME);
    }

    void out_port::stub() {
        VCML_ERROR_ON(m_stubbed, "port %s already stubbed", name());
        bind(m_stub);
        m_stubbed = true;
    }

}
