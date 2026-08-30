/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_UI_CONSOLE_H
#define VCML_UI_CONSOLE_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/ui/video.h"
#include "vcml/ui/keymap.h"
#include "vcml/ui/input.h"
#include "vcml/ui/backend.h"
#include "vcml/ui/display.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

namespace vcml {
namespace ui {

class console : public module, private display_if
{
private:
    size_t m_next_id;

    class display* m_display;
    unordered_set<input*> m_inputs;
    unordered_map<size_t, backend*> m_backends;

    class display* find_display(const string& name);
    class input* find_input(const string& name);

    size_t attach_backend(const string& type);
    bool detach_backend(size_t id);

    bool cmd_attach_backend(const vector<string>& args, ostream& os);
    bool cmd_detach_backend(const vector<string>& args, ostream& os);
    bool cmd_list_backends(const vector<string>& args, ostream& os);
    bool cmd_screenshot(const vector<string>& args, ostream& os);

public:
    property<string> display;
    property<vector<string>> inputs;
    property<vector<string>> backends;

    console(const sc_module_name& name);
    virtual ~console();
    VCML_KIND(ui::console);

protected:
    virtual void end_of_elaboration() override;
    virtual void end_of_simulation() override;

    virtual void display_setup(const videomode& mode, u8* fbptr) override;
    virtual void display_render(u32 x, u32 y, u32 w, u32 h) override;
};

} // namespace ui
} // namespace vcml

#endif
