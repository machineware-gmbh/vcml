
/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_STREAM_H
#define VCML_AUDIO_STREAM_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"

#include "vcml/logging/logger.h"

#include "vcml/audio/format.h"
#include "vcml/audio/driver.h"

namespace vcml {
namespace audio {

class stream : public module
{
protected:
    vector<driver*> m_drivers;

public:
    property<string> drivers;

    stream(const sc_core::sc_module_name& nm);
    virtual ~stream();

    virtual size_t min_channels() = 0;
    virtual size_t max_channels() = 0;

    virtual bool supports_format(u32 format) = 0;
    virtual bool supports_rate(u32 rate) = 0;

    virtual bool configure(u32 format, u32 channels, u32 rate) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;
};

} // namespace audio
} // namespace vcml

#endif
