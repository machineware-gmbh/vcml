/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_AUDIO_FORMAT_H
#define VCML_AUDIO_FORMAT_H

#include "vcml/core/types.h"

namespace vcml {
namespace audio {

enum audio_format : u32 {
    FORMAT_U8 = 0,
    FORMAT_S8,
    FORMAT_U16LE,
    FORMAT_S16LE,
    FORMAT_U16BE,
    FORMAT_S16BE,
    FORMAT_U32LE,
    FORMAT_S32LE,
    FORMAT_U32BE,
    FORMAT_S32BE,
    FORMAT_F32LE,
    FORMAT_F32BE,
};

const char* format_str(u32 format);
size_t format_bits(u32 format);
bool format_signed(u32 format);
bool format_float(u32 format);
bool format_native_endian(u32 format);

void fill_silence(void* buf, size_t len, u32 format);

} // namespace audio
} // namespace vcml

#endif
