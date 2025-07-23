
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

class ostream;

class driver
{
public:
    virtual ~driver() = default;

    virtual bool configure_output(u32 format, u32 channels, u32 rate) = 0;
    virtual void output(void* buf, size_t len) = 0;

    virtual void set_output_volume(float volume) = 0;

    static driver* create(ostream& owner, const string& type);
};

} // namespace audio
} // namespace vcml

#endif
