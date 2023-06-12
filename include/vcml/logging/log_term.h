/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOG_TERM_H
#define VCML_LOG_TERM_H

#include "vcml/core/types.h"
#include "vcml/logging/publisher.h"

namespace vcml {

class log_term : public publisher
{
private:
    bool m_colors;
    ostream& m_os;

public:
    bool has_colors() const { return m_colors; }
    void set_colors(bool set = true) { m_colors = set; }

    log_term(bool use_cerr = true);
    log_term(bool use_cerr, bool use_colors);
    virtual ~log_term();

    virtual void publish(const logmsg& msg) override;

    static const char* colors[NUM_LOG_LEVELS];
};

} // namespace vcml

#endif
