/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOGGING_REPORT_H
#define VCML_LOGGING_REPORT_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {
namespace publishers {

class report : public mwr::publisher
{
public:
    report();
    virtual ~report();

protected:
    virtual void publish(const mwr::logmsg& msg) override;
};

} // namespace publishers
} // namespace vcml

#endif
