/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_ISTREAM_H
#define VCML_AUDIO_ISTREAM_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"

#include "vcml/logging/logger.h"

#include "vcml/audio/format.h"
#include "vcml/audio/driver.h"
#include "vcml/audio/stream.h"

namespace vcml {
namespace audio {

class istream : public stream
{
public:
    istream(const sc_module_name& nm);
    virtual ~istream();

    virtual size_t min_channels() override;
    virtual size_t max_channels() override;

    virtual bool supports_format(u32 format) override;
    virtual bool supports_rate(u32 rate) override;

    virtual bool configure(u32 format, u32 channels, u32 rate) override;
    virtual void start() override;
    virtual void stop() override;
    virtual void shutdown() override;

    void xfer(void* buf, size_t len);

    template <typename T>
    void xfer(T& data) {
        xfer(&data, sizeof(T));
    }

    template <typename T>
    void xfer(vector<T>& v) {
        xfer(v.data(), v.size() * sizeof(T));
    }
};

} // namespace audio
} // namespace vcml

#endif
