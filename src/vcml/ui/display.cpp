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

#include "vcml/ui/display.h"

#ifdef HAVE_LIBVNC
#include "vcml/ui/vnc.h"
#endif

#ifdef HAVE_SDL2
#include "vcml/ui/sdl.h"
#endif

namespace vcml { namespace ui {

    unordered_map<string, shared_ptr<display>> display::displays = {
        { "", shared_ptr<display>(new display("", 0)) } // no-op server
    };

    display::display(const string& type, u32 no):
        m_mutex(),
        m_name(mkstr("%s:%u", type.c_str(), no)),
        m_type(type),
        m_dispno(no),
        m_mode(),
        m_fb(nullptr),
        m_key_listener(),
        m_ptr_listener() {
    }

    display::~display() {
        // nothing to do
    }

    void display::init(const fbmode& mode, u8* fbptr) {
        m_mode = mode;
        m_fb = fbptr;
    }

    void display::render() {
        // to be overloaded
    }

    void display::shutdown() {
        lock_guard<mutex> lock(m_mutex);
        m_key_listener.clear();
        m_ptr_listener.clear();
    }

    void display::add_key_listener(key_listener& l, const string& layout) {
        lock_guard<mutex> lock(m_mutex);
        if (stl_contains_if(m_key_listener,
            [l, layout](const key_listener_info& i) {
            return i.func == &l && i.layout == layout; })) {
            VCML_ERROR("key listener already registered");
        }

        m_key_listener.push_back({&l, layout});
    }

    void display::remove_key_listener(key_listener& listener) {
        lock_guard<mutex> lock(m_mutex);
        stl_remove_erase_if(m_key_listener, [listener](key_listener_info& i) {
            return i.func == &listener;
        });
    }

    void display::add_ptr_listener(ptr_listener& listener) {
        lock_guard<mutex> lock(m_mutex);
        if (stl_contains(m_ptr_listener, &listener))
            VCML_ERROR("pointer listener already registered");
        m_ptr_listener.push_back(&listener);
    }

    void display::remove_ptr_listener(ptr_listener& listener) {
        lock_guard<mutex> lock(m_mutex);
        stl_remove_erase(m_ptr_listener, &listener);
    }

    void display::notify_key_listeners(unsigned int key, bool down) {
        lock_guard<mutex> lock(m_mutex);
        for (auto listener : m_key_listener)
            (*listener.func)(key, down);
    }

    void display::notify_ptr_listeners(u32 buttons, u32 x, u32 y) {
        lock_guard<mutex> lock(m_mutex);
        for (auto listener : m_ptr_listener)
            (*listener)(buttons, x, y);
    }

    static bool parse_display(const string& name, string& id, u32& no) {
        auto it = name.rfind(":");
        if (it == string::npos)
            return false;

        id = name.substr(0, it);
        no = from_string<u32>(name.substr(it + 1, string::npos));

        return true;
    }

    shared_ptr<display> display::lookup(const string& name) {
        shared_ptr<display>& disp = displays[name];
        if (disp != nullptr)
            return disp;

        u32 no;
        string id;

        if (!parse_display(name, id, no))
            VCML_ERROR("cannot parse display name: %s", name.c_str());

        if (id == "display")
            disp.reset(new display("display", no));

        if (id == "vnc") {
#ifdef HAVE_LIBVNC
            disp.reset(new vnc(no));
#else
            disp.reset(new display("display", no));
#endif
        }

        if (id == "sdl") {
#ifdef HAVE_SDL2
            disp.reset(new sdl(no));
#else
            disp.reset(new display("display", no));
#endif
        }

        if (disp == nullptr)
            VCML_ERROR("display type '%s' not supported", id.c_str());

        return disp;
    }

}}
