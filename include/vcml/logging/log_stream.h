/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOG_STREAM_H
#define VCML_LOG_STREAM_H

#include "vcml/core/types.h"
#include "vcml/logging/publisher.h"

namespace vcml {

class log_stream : public publisher
{
protected:
    ostream& os;

public:
    log_stream(ostream& os);
    virtual ~log_stream();

    virtual void publish(const logmsg& msg) override;
};

} // namespace vcml

#endif
