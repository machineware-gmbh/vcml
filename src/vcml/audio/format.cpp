/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/audio/format.h"

namespace vcml {
namespace audio {

const char* format_str(u32 format) {
    switch (format) {
    case FORMAT_U8:
        return "FORMAT_U8";
    case FORMAT_S8:
        return "FORMAT_S8";
    case FORMAT_U16LE:
        return "FORMAT_U16LE";
    case FORMAT_S16LE:
        return "FORMAT_S16LE";
    case FORMAT_U16BE:
        return "FORMAT_U16BE";
    case FORMAT_S16BE:
        return "FORMAT_S16BE";
    case FORMAT_U32LE:
        return "FORMAT_U32LE";
    case FORMAT_S32LE:
        return "FORMAT_S32LE";
    case FORMAT_U32BE:
        return "FORMAT_U32BE";
    case FORMAT_S32BE:
        return "FORMAT_S32BE";
    case FORMAT_F32LE:
        return "FORMAT_F32LE";
    case FORMAT_F32BE:
        return "FORMAT_F32BE";
    default:
        VCML_ERROR("invalid audio format: 0x%x", format);
    }
}

size_t format_bits(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_S8:
        return 8;
    case FORMAT_U16LE:
    case FORMAT_S16LE:
    case FORMAT_U16BE:
    case FORMAT_S16BE:
        return 16;
    case FORMAT_U32LE:
    case FORMAT_S32LE:
    case FORMAT_U32BE:
    case FORMAT_S32BE:
    case FORMAT_F32LE:
    case FORMAT_F32BE:
        return 32;
    default:
        VCML_ERROR("invalid audio format: 0x%x", format);
    }
}

bool format_signed(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_U16LE:
    case FORMAT_U16BE:
    case FORMAT_U32LE:
    case FORMAT_U32BE:
        return false;

    case FORMAT_S8:
    case FORMAT_S16LE:
    case FORMAT_S16BE:
    case FORMAT_S32LE:
    case FORMAT_S32BE:
    case FORMAT_F32LE:
    case FORMAT_F32BE:
        return true;

    default:
        VCML_ERROR("invalid audio format: 0x%x", format);
    }
}

bool format_float(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_S8:
    case FORMAT_U16LE:
    case FORMAT_S16LE:
    case FORMAT_U16BE:
    case FORMAT_S16BE:
    case FORMAT_U32LE:
    case FORMAT_S32LE:
    case FORMAT_U32BE:
    case FORMAT_S32BE:
        return false;

    case FORMAT_F32LE:
    case FORMAT_F32BE:
        return true;

    default:
        VCML_ERROR("invalid audio format: 0x%x", format);
    }
}

bool format_native_endian(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_S8:
        return true;

    case FORMAT_U16LE:
    case FORMAT_S16LE:
    case FORMAT_U32LE:
    case FORMAT_S32LE:
    case FORMAT_F32LE:
        return host_endian() == ENDIAN_LITTLE;

    case FORMAT_U16BE:
    case FORMAT_S16BE:
    case FORMAT_U32BE:
    case FORMAT_S32BE:
    case FORMAT_F32BE:
        return host_endian() == ENDIAN_BIG;

    default:
        VCML_ERROR("invalid audio format: 0x%x", format);
    }
}

void fill_silence(void* buf, size_t len, u32 format) {
    switch (format_bits(format)) {
    case 8: {
        u8 fill = format_signed(format) ? 0 : 0x7f;
        memset(buf, fill, len);
        break;
    }

    case 16: {
        u16 fill = format_signed(format) ? 0 : 0x7fff;
        if (!format_native_endian(format))
            fill = bswap(fill);
        for (size_t i = 0; i < len; i += sizeof(u16))
            *(u16*)((u8*)buf + i) = fill;
        break;
    }

    case 32: {
        u32 fill = format_signed(format) ? 0 : 0x7fffffff;
        if (!format_native_endian(format))
            fill = bswap(fill);
        for (size_t i = 0; i < len; i += sizeof(u32))
            *(u32*)((u8*)buf + i) = fill;
        break;
    }

    default:
        VCML_ERROR("unsupported format: 0x%x", format);
    }
}

} // namespace audio
} // namespace vcml
