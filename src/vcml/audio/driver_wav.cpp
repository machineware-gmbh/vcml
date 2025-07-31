/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/audio/driver_wav.h"

namespace vcml {
namespace audio {

// Audio format codes
enum wav_audio_format : u16 {
    WAV_PCM = 0x0001,
    WAV_FLOAT = 0x0003,
};

#pragma pack(push, 1)
struct wav_file_header {
    u32 chunk_id;
    u32 chunk_size;
    u32 format;

    u32 subchunk1_id;
    u32 subchunk1_size;
    u16 audio_format;
    u16 num_channels;
    u32 sample_rate;
    u32 byte_rate;
    u16 block_align;
    u16 bits_per_sample;

    u32 subchunk2_id;
    u32 subchunk2_size;
};
#pragma pack(pop)

static u32 format_from_wav(const wav_file_header& hdr) {
    if (hdr.audio_format == WAV_PCM) {
        switch (hdr.bits_per_sample) {
        case 8:
            return FORMAT_U8;
        case 16:
            return FORMAT_S16LE;
        case 32:
            return FORMAT_S32LE;
        default:
            return FORMAT_INVALID;
        }
    }

    if (hdr.audio_format == WAV_FLOAT && hdr.bits_per_sample == 32)
        return FORMAT_F32LE;

    return FORMAT_INVALID;
}

static void wav_update_size(fstream& file) {
    if (!file)
        return;

    file.seekg(0, std::ios::end);
    size_t size = (u32)file.tellg();

    u32 riff_size = size - 8;
    file.seekp(4, std::ios::beg);
    file.write((const char*)&riff_size, sizeof(riff_size));

    u32 data_size = size - 44;
    file.seekp(40, std::ios::beg);
    file.write((const char*)&data_size, sizeof(data_size));

    file.seekp(0, std::ios::end);
}

void driver_wav::handle_option(const string& option) {
    // setting file path is the only option we have
    m_path = option;
    log_debug("using file %s", m_path.c_str());
}

void driver_wav::load_input_params() {
    fstream input(m_path, std::ios::binary | std::ios::in);
    wav_file_header hdr{};
    input.read((char*)&hdr, sizeof(hdr));
    input.seekg(0, std::ios::end);

    if (input)
        m_input_size = input.tellg();

    if (m_input_size > sizeof(wav_file_header)) {
        m_input_format = format_from_wav(hdr);
        m_input_channels = hdr.num_channels;
        m_input_rate = hdr.sample_rate;
    } else {
        m_input_size = 0;
        m_input_format = FORMAT_INVALID;
        m_input_channels = 0;
        m_input_rate = 0;
    }
}

driver_wav::driver_wav(stream& owner, const string& type):
    driver(owner),
    m_path(mkstr("%s.wav", owner.name())),
    m_output(),
    m_output_enabled(),
    m_input(),
    m_input_size(),
    m_input_format(FORMAT_INVALID),
    m_input_channels(),
    m_input_rate(),
    m_input_enabled() {
    auto options = split(type, ':');
    for (size_t i = 1; i < options.size(); i++)
        handle_option(options[i]);

    load_input_params();
}

driver_wav::~driver_wav() {
    // TODO
}

size_t driver_wav::output_min_channels() {
    return 1;
}

size_t driver_wav::output_max_channels() {
    return 2;
}

bool driver_wav::output_supports_format(u32 format) {
    switch (format) {
    case FORMAT_U8:
    case FORMAT_S16LE:
    case FORMAT_S32LE:
    case FORMAT_F32LE:
        return true;

    case FORMAT_S8:
    case FORMAT_U16LE:
    case FORMAT_U16BE:
    case FORMAT_S16BE:
    case FORMAT_U32LE:
    case FORMAT_U32BE:
    case FORMAT_S32BE:
    case FORMAT_F32BE:
    default:
        return false;
    }
}

bool driver_wav::output_supports_rate(u32 rate) {
    return true;
}

bool driver_wav::output_configure(u32 format, u32 channels, u32 rate) {
    m_output.close();

    if (!output_supports_format(format))
        return false;

    m_output.open(m_path, std::ios::binary | std::ios::out);
    if (!m_output)
        return false;

    wav_file_header hdr{};
    hdr.chunk_id = fourcc("RIFF");
    hdr.chunk_size = 0;
    hdr.format = fourcc("WAVE");

    hdr.subchunk1_id = fourcc("fmt ");
    hdr.subchunk1_size = 16;
    hdr.audio_format = format_is_float(format) ? WAV_FLOAT : WAV_PCM;
    hdr.num_channels = channels;
    hdr.sample_rate = rate;
    hdr.byte_rate = rate * channels * format_bits(format) / 8;
    hdr.block_align = channels * format_bits(format) / 8;
    hdr.bits_per_sample = format_bits(format);

    hdr.subchunk2_id = fourcc("data");
    hdr.subchunk2_size = 0;

    m_output.write((char*)&hdr, sizeof(hdr));

    return true;
}

void driver_wav::output_enable(bool enable) {
    m_output_enabled = enable;
}

void driver_wav::output_xfer(const void* buf, size_t len) {
    if (m_output && m_output_enabled) {
        m_output.write((const char*)buf, len);
        wav_update_size(m_output);
    }
}

void driver_wav::output_shutdown() {
    if (m_output) {
        wav_update_size(m_output);
        m_output.close();
    }
}

size_t driver_wav::input_min_channels() {
    return m_input_channels;
}

size_t driver_wav::input_max_channels() {
    return m_input_channels;
}

bool driver_wav::input_supports_format(u32 format) {
    return format == m_input_format;
}

bool driver_wav::input_supports_rate(u32 rate) {
    return rate == m_input_rate;
}

bool driver_wav::input_configure(u32 format, u32 channels, u32 rate) {
    if (format != m_input_format)
        return false;
    if (channels != m_input_channels)
        return false;
    if (rate != m_input_rate)
        return false;

    m_input.open(m_path, std::ios::binary | std::ios::in);
    if (!m_input)
        return false;

    m_input.seekg(sizeof(wav_file_header), std::ios::beg);
    if (!m_input || (size_t)m_input.tellg() >= m_input_size || m_input.eof()) {
        m_input.close();
        return false;
    }

    return true;
}

void driver_wav::input_enable(bool enable) {
    m_input_enabled = enable;
}

void driver_wav::input_xfer(void* buf, size_t len) {
    size_t done = 0;
    while (m_input && m_input_enabled && done < len) {
        char* dest = (char*)buf + done;
        size_t num = min<size_t>(len - done, m_input_size - m_input.tellg());

        if (num == 0) {
            // restart file from the beginning
            m_input.seekg(sizeof(wav_file_header), std::ios::beg);
            break;
        }

        m_input.read(dest, num);
        done += num;
    }

    if (done < len)
        fill_silence((u8*)buf + done, len - done, m_input_format);
}

void driver_wav::input_shutdown() {
    if (m_input)
        m_input.close();
}

} // namespace audio
} // namespace vcml
