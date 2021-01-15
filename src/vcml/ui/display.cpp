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

    display::display(const string& type, u32 nr):
        m_name(mkstr("%s:%u", type.c_str(), nr)),
        m_type(type),
        m_dispno(nr),
        m_mode(),
        m_fb(nullptr) {
    }

    display::~display() {
        // nothing to do
    }

    void display::init(const fbmode& mode, u8* fbptr) {
        m_mode = mode;
        m_fb = fbptr;
    }

    void display::render() {
        // nothing to do
    }

    void display::shutdown() {
        // nothing to do
    }

    void display::add_key_listener(key_listener& l, const string& layout) {
        // nothing to do
    }

    void display::add_ptr_listener(pos_listener& p, key_listener& k) {
        // nothing to do
    }

    void display::remove_key_listener(key_listener& l) {
        // nothing to do
    }

    void display::remove_ptr_listener(pos_listener& p, key_listener& k) {
        // nothing to do
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
