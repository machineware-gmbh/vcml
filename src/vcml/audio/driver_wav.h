/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_DRIVER_WAV_H
#define VCML_AUDIO_DRIVER_WAV_H

#include "vcml/audio/driver.h"

namespace vcml {
namespace audio {

class driver_wav : public driver
{
private:
    string m_path;
    ofstream m_file;

public:
    driver_wav(const string& path);
    virtual ~driver_wav();

    virtual bool configure_output(u32 format, u32 channels, u32 rate) override;
    virtual void output(void* buf, size_t len) override;
    virtual void set_output_volume(float volume) override;
};

} // namespace audio
} // namespace vcml

#endif
