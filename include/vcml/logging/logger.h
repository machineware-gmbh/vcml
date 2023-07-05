/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOGGING_LOGGER_H
#define VCML_LOGGING_LOGGER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

class logger : public mwr::logger
{
private:
    class module* m_parent;

public:
    virtual bool can_log(log_level lvl) const;

    logger();
    logger(sc_object* parent);
    logger(const string& name);

    logger(logger&&) = default;
    logger(const logger&) = default;
};

extern logger log;

} // namespace vcml

#endif
