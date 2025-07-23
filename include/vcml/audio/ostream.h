
/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_OSTREAM_H
#define VCML_AUDIO_OSTREAM_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"

#include "vcml/logging/logger.h"

#include "vcml/audio/format.h"
#include "vcml/audio/driver.h"

namespace vcml {
namespace audio {

class ostream : public module
{
private:
    vector<driver*> m_drivers;

    bool cmd_set_volume(const vector<string>& args, std::ostream& os);

public:
    property<string> drivers;

    ostream(const sc_core::sc_module_name& nm);
    virtual ~ostream();

    bool configure(u32 format, u32 channels, u32 rate);
    void output(void* buf, size_t len);
    void set_volume(float volume);
};

} // namespace audio
} // namespace vcml

#endif
