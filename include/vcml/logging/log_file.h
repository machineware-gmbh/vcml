/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOG_FILE_H
#define VCML_LOG_FILE_H

#include "vcml/core/types.h"
#include "vcml/logging/publisher.h"

namespace vcml {

class log_file : public publisher
{
private:
    ofstream m_file;

public:
    log_file(const string& filename);
    virtual ~log_file();

    virtual void publish(const logmsg& msg) override;
};

} // namespace vcml

#endif
