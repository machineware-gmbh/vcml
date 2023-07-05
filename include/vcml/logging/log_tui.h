/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOG_TUI_H
#define VCML_LOG_TUI_H

#include "vcml/core/types.h"
#include "mwr/logging/publisher.h"

#include "vcml/ui/tui.h"

namespace vcml {

class log_tui : public mwr::publisher
{
private:
    bool m_colors;

public:
    bool has_colors() const { return m_colors; }
    void set_colors(bool set = true) { m_colors = set; }

    log_tui() = delete;
    log_tui(bool use_colors);
    virtual ~log_tui() = default;

    virtual void publish(const mwr::logmsg& msg) override;

    static const int COLORS[mwr::log_level::NUM_LOG_LEVELS];
};

} // namespace vcml

#endif
