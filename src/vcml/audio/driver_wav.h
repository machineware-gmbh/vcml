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

#include "vcml/core/types.h"
#include "vcml/core/module.h"

#include "vcml/audio/driver.h"
#include "vcml/audio/stream.h"

namespace vcml {
namespace audio {

class driver_wav : public driver
{
private:
    string m_path;
    fstream m_output;
    bool m_output_enabled;

    fstream m_input;
    size_t m_input_size;
    u32 m_input_format;
    u32 m_input_channels;
    u32 m_input_rate;
    bool m_input_enabled;

    void handle_option(const string& option);
    void load_input_params();

public:
    driver_wav(stream& owner, const string& type);
    virtual ~driver_wav();

    virtual size_t output_min_channels() override;
    virtual size_t output_max_channels() override;
    virtual bool output_supports_format(u32 format) override;
    virtual bool output_supports_rate(u32 rate) override;
    virtual bool output_configure(u32 format, u32 channels, u32 rate) override;
    virtual void output_enable(bool enable) override;
    virtual void output_xfer(const void* buf, size_t len) override;
    virtual void output_shutdown() override;

    virtual size_t input_min_channels() override;
    virtual size_t input_max_channels() override;
    virtual bool input_supports_format(u32 format) override;
    virtual bool input_supports_rate(u32 rate) override;
    virtual bool input_configure(u32 format, u32 channels, u32 rate) override;
    virtual void input_enable(bool enable) override;
    virtual void input_xfer(void* buf, size_t len) override;
    virtual void input_shutdown() override;
};

} // namespace audio
} // namespace vcml

#endif
