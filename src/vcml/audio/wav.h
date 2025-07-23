/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_WAV_H
#define VCML_AUDIO_WAV_H

#include "vcml/audio/driver.h"

namespace vcml {
namespace audio {

class wav_driver : public driver
{
private:
    string m_path;
    ofstream m_file;

public:
    wav_driver(const string& path): m_path(path), m_file() {}

    virtual ~wav_driver() = default;

    virtual bool configure_output(u32 format, u32 channels,
                                  u32 rate) override {
        m_file.open(m_path);
        if (!m_file)
            return false;

        return true;
    }

    virtual void output(void* buf, size_t len) override {}

    virtual void set_output_volume(float volume) override {
        // nothing to do
    }
};

} // namespace audio
} // namespace vcml

#endif
