
/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_DRIVER_H
#define VCML_AUDIO_DRIVER_H

#include "vcml/core/types.h"
#include "vcml/audio/format.h"

namespace vcml {
namespace audio {

class stream;

class driver
{
protected:
    stream& m_stream;

public:
    mwr::logger& log;

    driver(stream& owner);
    virtual ~driver();

    virtual size_t output_min_channels() = 0;
    virtual size_t output_max_channels() = 0;
    virtual bool output_supports_format(u32 format) = 0;
    virtual bool output_supports_rate(u32 rate) = 0;
    virtual bool output_configure(u32 format, u32 channels, u32 rate) = 0;
    virtual void output_enable(bool enable) = 0;
    virtual void output_xfer(const void* buf, size_t len) = 0;
    virtual void output_shutdown() = 0;

    virtual size_t input_min_channels() = 0;
    virtual size_t input_max_channels() = 0;
    virtual bool input_supports_format(u32 format) = 0;
    virtual bool input_supports_rate(u32 rate) = 0;
    virtual bool input_configure(u32 format, u32 channels, u32 rate) = 0;
    virtual void input_enable(bool enable) = 0;
    virtual void input_xfer(void* buf, size_t len) = 0;
    virtual void input_shutdown() = 0;

    static driver* create(stream& owner, const string& type);
};

} // namespace audio
} // namespace vcml

#endif
