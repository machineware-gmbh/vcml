/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/ui/console.h"
#include "vcml/core/module.h"

namespace vcml {
namespace ui {

display* console::find_display(const string& name) {
    auto* disp = dynamic_cast<class display*>(vcml::find_object(name));
    if (!disp)
        log_error("display not found: %s", name.c_str());
    return disp;
}

input* console::find_input(const string& name) {
    input* inp = dynamic_cast<input*>(vcml::find_object(name));
    if (!inp)
        log_error("input not found: %s", name.c_str());
    return inp;
}

size_t console::attach_backend(const string& desc) {
    auto* backend = ui::backend::create(desc);
    m_backends[m_next_id] = backend;
    for (auto* input : m_inputs)
        backend->attach(input);
    return m_next_id++;
}

bool console::detach_backend(size_t id) {
    auto it = m_backends.find(id);
    if (it == m_backends.end())
        return false;
    ui::backend::destroy(it->second);
    m_backends.erase(it);
    return true;
}

bool console::cmd_attach_backend(const vector<string>& args, ostream& os) {
    try {
        size_t id = attach_backend(args[0]);
        os << "created backend " << id;
        return true;
    } catch (std::exception& ex) {
        os << "error creating backend " << args[0] << ":" << ex.what();
        return false;
    }
}

bool console::cmd_detach_backend(const vector<string>& args, ostream& os) {
    for (const string& arg : args) {
        if (to_lower(arg) == "all") {
            m_backends.clear();
            return true;
        } else {
            size_t id = from_string<size_t>(arg);
            if (!detach_backend(id))
                os << "invalid backend id: " << id;
        }
    }

    return true;
}

bool console::cmd_list_backends(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        for (auto& it : m_backends)
            os << it.first << ": " << it.second->type() << ",";
        return true;
    }

    for (const string& arg : args) {
        size_t id = from_string<size_t>(arg);

        auto it = m_backends.find(id);
        os << id << ": ";
        os << (it == m_backends.end() ? "none" : it->second->type());
        os << ",";
    }

    return true;
}

bool console::cmd_screenshot(const vector<string>& args, ostream& os) {
    if (!m_display) {
        os << "no display to take screenshot from";
        return false;
    }

    string path = mkstr("%s.bmp", name());
    if (args.size() > 0)
        path = args[0];

    if (m_display->screenshot(path)) {
        os << "screenshot stored in '" << path << "'";
        return true;
    } else {
        os << "failed to store screenshot in '" << path << "'";
        return false;
    }
}

console::console(const sc_module_name& nm):
    module(nm),
    display_if(),
    m_next_id(),
    m_display(),
    m_inputs(),
    m_backends(),
    display("display"),
    inputs("inputs"),
    backends("backends") {
    register_command(
        "attach_backend", 1, &console::cmd_attach_backend,
        "creates a new UI backend of a given type and attaches it to this "
        "console, usage: attach_backend <type>");
    register_command(
        "detach_backend", 1, &console::cmd_detach_backend,
        "disconnects a UI backend from this console with the given IDs, "
        "usage: detach_backend <ID> [ID]..| all");
    register_command(
        "list_backends", 0, &console::cmd_list_backends,
        "lists all known UI backends that are attached to this console");
    register_command(
        "screenshot", 0, &console::cmd_screenshot,
        "store a screenshot of the framebuffer, usage: screenshot [path]");
}

console::~console() {
    // fallback, if end_of_simulation did not get called
    for (auto [id, backend] : m_backends)
        ui::backend::destroy(backend);
}

void console::end_of_elaboration() {
    if (display.empty() && inputs.empty()) {
        log_debug("no display and no inputs, going idle");
        return;
    }

    for (const string& input : inputs)
        m_inputs.insert(find_input(input));

    for (const string& desc : backends) {
        try {
            attach_backend(desc);
        } catch (std::exception& ex) {
            log_error("error creating backend: %s", ex.what());
        }
    }

    m_display = find_display(display);
    if (m_display)
        m_display->attach(this);
}

void console::end_of_simulation() {
    for (auto [_, b] : m_backends) {
        for (auto* input : m_inputs)
            b->detach(input);
    }

    if (m_display)
        m_display->detach(this);

    for (auto [_, backend] : m_backends)
        ui::backend::destroy(backend);
    m_backends.clear();
}

void console::display_setup(const videomode& mode, u8* fbptr) {
    for (auto [_, backend] : m_backends)
        backend->setup(mode, fbptr);
}

void console::display_render(u32 x, u32 y, u32 w, u32 h) {
    for (auto [_, backend] : m_backends)
        backend->render(x, y, w, h);
}

VCML_EXPORT_MODEL(vcml::ui::console, name, args) {
    return new console(name);
}

} // namespace ui
} // namespace vcml
