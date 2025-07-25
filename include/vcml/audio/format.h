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

#include "vcml/core/systemc.h"
#include "vcml/core/types.h"

namespace vcml {
namespace audio {

enum audio_format : u32 {
    AUDIO_8BIT = 0 << 0,
    AUDIO_16BIT = 1 << 0,
    AUDIO_32BIT = 2 << 0,
    AUDIO_64BIT = 3 << 0,
    AUDIO_UNSIGNED = 0 << 2,
    AUDIO_SIGNED = 1 << 2,
    AUDIO_FLOAT = 2 << 2,
    AUDIO_ENDIAN_LITTLE = 0 << 4,
    AUDIO_ENDIAN_BIG = 1 << 4,

    FORMAT_U8 = AUDIO_8BIT | AUDIO_UNSIGNED,
    FORMAT_S8 = AUDIO_8BIT | AUDIO_SIGNED,

    FORMAT_U16LE = AUDIO_16BIT | AUDIO_UNSIGNED | AUDIO_ENDIAN_LITTLE,
    FORMAT_U16BE = AUDIO_16BIT | AUDIO_UNSIGNED | AUDIO_ENDIAN_BIG,
    FORMAT_S16LE = AUDIO_16BIT | AUDIO_SIGNED | AUDIO_ENDIAN_LITTLE,
    FORMAT_S16BE = AUDIO_16BIT | AUDIO_SIGNED | AUDIO_ENDIAN_BIG,

    FORMAT_U32LE = AUDIO_32BIT | AUDIO_UNSIGNED | AUDIO_ENDIAN_LITTLE,
    FORMAT_U32BE = AUDIO_32BIT | AUDIO_UNSIGNED | AUDIO_ENDIAN_BIG,
    FORMAT_S32LE = AUDIO_32BIT | AUDIO_SIGNED | AUDIO_ENDIAN_LITTLE,
    FORMAT_S32BE = AUDIO_32BIT | AUDIO_SIGNED | AUDIO_ENDIAN_BIG,

    FORMAT_F32LE = AUDIO_32BIT | AUDIO_FLOAT | AUDIO_ENDIAN_LITTLE,
    FORMAT_F32BE = AUDIO_32BIT | AUDIO_FLOAT | AUDIO_ENDIAN_BIG,

    FORMAT_INVALID = ~0u,
};

constexpr size_t format_bits(u32 format) {
    return 8u << (format & 0b11);
}

constexpr bool format_is_unsigned(u32 format) {
    return (format & 0b1100) == AUDIO_UNSIGNED;
}

constexpr bool format_is_signed(u32 format) {
    return !format_is_unsigned(format);
}

constexpr bool format_is_float(u32 format) {
    return (format & 0b1100) == AUDIO_FLOAT;
}

constexpr bool format_is_big_endian(u32 format) {
    return (format & 0b10000) == AUDIO_ENDIAN_BIG;
}

constexpr bool format_is_little_endian(u32 format) {
    return (format & 0b10000) == AUDIO_ENDIAN_LITTLE;
}

constexpr bool format_is_native_endian(u32 format) {
    if (host_endian() == ENDIAN_LITTLE)
        return format_is_little_endian(format);
    if (host_endian() == ENDIAN_BIG)
        return format_is_big_endian(format);
    return false;
}

const char* format_str(u32 format);

void fill_silence(void* buf, size_t len, u32 format);

constexpr size_t buffer_size(time_t ms, u32 format, u32 channels, u32 rate) {
    size_t frame_size = format_bits(format) / 8 * channels;
    return (frame_size * rate * ms) / 1000;
}

} // namespace audio
} // namespace vcml

#endif
