/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/virtio/sound.h"

namespace vcml {
namespace virtio {

enum virtio_snd_streamids : u32 {
    STREAMID_TX = 0,
    STREAMID_RX,
    STREAMID_NUM,
};

struct virtio_snd_hdr {
    u32 code;
};

struct virtio_snd_event {
    virtio_snd_hdr hdr;
    u32 data;
};

struct virtio_snd_query_info {
    virtio_snd_hdr hdr;
    u32 start_id;
    u32 count;
    u32 size;
};

struct virtio_snd_info {
    u32 hda_fn_nid;
};

struct virtio_snd_pcm_hdr {
    virtio_snd_hdr hdr;
    u32 stream_id;
};

enum virtio_snd_pcm_formats : u32 {
    VIRTIO_SND_PCM_FMT_IMA_ADPCM = 0,
    VIRTIO_SND_PCM_FMT_MU_LAW,
    VIRTIO_SND_PCM_FMT_A_LAW,
    VIRTIO_SND_PCM_FMT_S8,
    VIRTIO_SND_PCM_FMT_U8,
    VIRTIO_SND_PCM_FMT_S16,
    VIRTIO_SND_PCM_FMT_U16,
    VIRTIO_SND_PCM_FMT_S18_3,
    VIRTIO_SND_PCM_FMT_U18_3,
    VIRTIO_SND_PCM_FMT_S20_3,
    VIRTIO_SND_PCM_FMT_U20_3,
    VIRTIO_SND_PCM_FMT_S24_3,
    VIRTIO_SND_PCM_FMT_U24_3,
    VIRTIO_SND_PCM_FMT_S20,
    VIRTIO_SND_PCM_FMT_U20,
    VIRTIO_SND_PCM_FMT_S24,
    VIRTIO_SND_PCM_FMT_U24,
    VIRTIO_SND_PCM_FMT_S32,
    VIRTIO_SND_PCM_FMT_U32,
    VIRTIO_SND_PCM_FMT_FLOAT,
    VIRTIO_SND_PCM_FMT_FLOAT64,
    VIRTIO_SND_PCM_FMT_DSD_U8,
    VIRTIO_SND_PCM_FMT_DSD_U16,
    VIRTIO_SND_PCM_FMT_DSD_U32,
    VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME,
    VIRTIO_SND_PCM_FMT_NUM,
};

static const char* virtio_snd_pcm_format_str(u32 format) {
    switch (format) {
    case VIRTIO_SND_PCM_FMT_IMA_ADPCM:
        return "VIRTIO_SND_PCM_FMT_IMA_ADPCM";
    case VIRTIO_SND_PCM_FMT_MU_LAW:
        return "VIRTIO_SND_PCM_FMT_MU_LAW";
    case VIRTIO_SND_PCM_FMT_A_LAW:
        return "VIRTIO_SND_PCM_FMT_A_LAW";
    case VIRTIO_SND_PCM_FMT_S8:
        return "VIRTIO_SND_PCM_FMT_S8";
    case VIRTIO_SND_PCM_FMT_U8:
        return "VIRTIO_SND_PCM_FMT_U8";
    case VIRTIO_SND_PCM_FMT_S16:
        return "VIRTIO_SND_PCM_FMT_S16";
    case VIRTIO_SND_PCM_FMT_U16:
        return "VIRTIO_SND_PCM_FMT_U16";
    case VIRTIO_SND_PCM_FMT_S18_3:
        return "VIRTIO_SND_PCM_FMT_S18_3";
    case VIRTIO_SND_PCM_FMT_U18_3:
        return "VIRTIO_SND_PCM_FMT_U18_3";
    case VIRTIO_SND_PCM_FMT_S20_3:
        return "VIRTIO_SND_PCM_FMT_S20_3";
    case VIRTIO_SND_PCM_FMT_U20_3:
        return "VIRTIO_SND_PCM_FMT_U20_3";
    case VIRTIO_SND_PCM_FMT_S24_3:
        return "VIRTIO_SND_PCM_FMT_S24_3";
    case VIRTIO_SND_PCM_FMT_U24_3:
        return "VIRTIO_SND_PCM_FMT_U24_3";
    case VIRTIO_SND_PCM_FMT_S20:
        return "VIRTIO_SND_PCM_FMT_S20";
    case VIRTIO_SND_PCM_FMT_U20:
        return "VIRTIO_SND_PCM_FMT_U20";
    case VIRTIO_SND_PCM_FMT_S24:
        return "VIRTIO_SND_PCM_FMT_S24";
    case VIRTIO_SND_PCM_FMT_U24:
        return "VIRTIO_SND_PCM_FMT_U24";
    case VIRTIO_SND_PCM_FMT_S32:
        return "VIRTIO_SND_PCM_FMT_S32";
    case VIRTIO_SND_PCM_FMT_U32:
        return "VIRTIO_SND_PCM_FMT_U32";
    case VIRTIO_SND_PCM_FMT_FLOAT:
        return "VIRTIO_SND_PCM_FMT_FLOAT";
    case VIRTIO_SND_PCM_FMT_FLOAT64:
        return "VIRTIO_SND_PCM_FMT_FLOAT64";
    case VIRTIO_SND_PCM_FMT_DSD_U8:
        return "VIRTIO_SND_PCM_FMT_DSD_U8";
    case VIRTIO_SND_PCM_FMT_DSD_U16:
        return "VIRTIO_SND_PCM_FMT_DSD_U16";
    case VIRTIO_SND_PCM_FMT_DSD_U32:
        return "VIRTIO_SND_PCM_FMT_DSD_U32";
    case VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME:
        return "VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME";
    default:
        return "unkown";
    }
}

static size_t virtio_snd_pcm_format_bps(u32 format) {
    switch (format) {
    case VIRTIO_SND_PCM_FMT_IMA_ADPCM:
    case VIRTIO_SND_PCM_FMT_MU_LAW:
    case VIRTIO_SND_PCM_FMT_A_LAW:
    case VIRTIO_SND_PCM_FMT_S8:
    case VIRTIO_SND_PCM_FMT_U8:
    case VIRTIO_SND_PCM_FMT_DSD_U8:
        return 1;
    case VIRTIO_SND_PCM_FMT_S16:
    case VIRTIO_SND_PCM_FMT_U16:
    case VIRTIO_SND_PCM_FMT_DSD_U16:
        return 2;
    case VIRTIO_SND_PCM_FMT_S18_3:
    case VIRTIO_SND_PCM_FMT_U18_3:
    case VIRTIO_SND_PCM_FMT_S20_3:
    case VIRTIO_SND_PCM_FMT_U20_3:
    case VIRTIO_SND_PCM_FMT_S24_3:
    case VIRTIO_SND_PCM_FMT_U24_3:
    case VIRTIO_SND_PCM_FMT_S20:
    case VIRTIO_SND_PCM_FMT_U20:
    case VIRTIO_SND_PCM_FMT_S24:
    case VIRTIO_SND_PCM_FMT_U24:
        return 3;
    case VIRTIO_SND_PCM_FMT_S32:
    case VIRTIO_SND_PCM_FMT_U32:
    case VIRTIO_SND_PCM_FMT_FLOAT:
    case VIRTIO_SND_PCM_FMT_DSD_U32:
    case VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME:
        return 4;
    case VIRTIO_SND_PCM_FMT_FLOAT64:
        return 8;
    default:
        VCML_ERROR("invalid sound format: 0x%x", format);
    }
}

static u32 virtio_snd_pcm_format_to_vcml(u32 format) {
    switch (format) {
    case VIRTIO_SND_PCM_FMT_S8:
        return audio::FORMAT_S8;
    case VIRTIO_SND_PCM_FMT_U8:
        return audio::FORMAT_U8;
    case VIRTIO_SND_PCM_FMT_S16:
        return audio::FORMAT_S16LE;
    case VIRTIO_SND_PCM_FMT_U16:
        return audio::FORMAT_U16LE;
    case VIRTIO_SND_PCM_FMT_S32:
        return audio::FORMAT_S32LE;
    case VIRTIO_SND_PCM_FMT_U32:
        return audio::FORMAT_U32LE;
    case VIRTIO_SND_PCM_FMT_FLOAT:
        return audio::FORMAT_F32LE;
    default:
        return audio::FORMAT_INVALID;
    }
}

enum virtio_snd_pcm_rates : u32 {
    VIRTIO_SND_PCM_RATE_5512 = 0,
    VIRTIO_SND_PCM_RATE_8000,
    VIRTIO_SND_PCM_RATE_11025,
    VIRTIO_SND_PCM_RATE_16000,
    VIRTIO_SND_PCM_RATE_22050,
    VIRTIO_SND_PCM_RATE_32000,
    VIRTIO_SND_PCM_RATE_44100,
    VIRTIO_SND_PCM_RATE_48000,
    VIRTIO_SND_PCM_RATE_64000,
    VIRTIO_SND_PCM_RATE_88200,
    VIRTIO_SND_PCM_RATE_96000,
    VIRTIO_SND_PCM_RATE_176400,
    VIRTIO_SND_PCM_RATE_192000,
    VIRTIO_SND_PCM_RATE_384000,
    VIRTIO_SND_PCM_RATE_NUM,
};

static size_t virtio_snd_pcm_rate_hz(u32 rate) {
    static const size_t rates[VIRTIO_SND_PCM_RATE_NUM] = {
        5512,  8000,  11025, 16000, 22050,  32000,  44100,
        48000, 64000, 88200, 96000, 176400, 192000, 384000
    };
    return rate < VIRTIO_SND_PCM_RATE_NUM ? rates[rate] : -1;
}

static sc_time virtio_snd_buffer_duration(size_t length, u32 format,
                                          u32 channels, u32 rate) {
    size_t hz = virtio_snd_pcm_rate_hz(rate);
    size_t bps = virtio_snd_pcm_format_bps(format);
    size_t num_samples = length / bps;
    size_t num_frames = num_samples / channels;
    return sc_time(num_frames * 1.0 / hz, SC_SEC);
}

static u64 virtio_snd_supported_formats(audio::stream& stream) {
    u64 formats = 0;
    for (u32 format = 0; format < VIRTIO_SND_PCM_FMT_NUM; format++) {
        u32 vcml = virtio_snd_pcm_format_to_vcml(format);
        if (stream.supports_format(vcml))
            formats |= bit(format);
    }

    return formats;
}

static u32 virtio_snd_supported_rates(audio::stream& stream) {
    u64 rates = 0;
    for (u32 rate = 0; rate < VIRTIO_SND_PCM_RATE_NUM; rate++) {
        u32 hz = virtio_snd_pcm_rate_hz(rate);
        if (stream.supports_rate(hz))
            rates |= bit(rate);
    }

    return rates;
}

enum virtio_snd_directions {
    VIRTIO_SND_D_OUTPUT = 0,
    VIRTIO_SND_D_INPUT,
};

struct virtio_snd_pcm_info {
    virtio_snd_info hdr;
    u32 features;
    u64 formats;
    u64 rates;
    u8 direction;
    u8 channels_min;
    u8 channels_max;
    u8 padding[5];
};

struct virtio_snd_pcm_set_params {
    virtio_snd_pcm_hdr hdr;
    u32 buffer_bytes;
    u32 period_bytes;
    u32 features;
    u8 channels;
    u8 format;
    u8 rate;
    u8 padding;
};

struct virtio_snd_pcm_xfer {
    u32 stream_id;
};

struct virtio_snd_pcm_status {
    u32 status;
    u32 latency_bytes;
};

enum virtio_snd_codes : u32 {
    // jack control request types
    VIRTIO_SND_R_JACK_INFO = 1,
    VIRTIO_SND_R_JACK_REMAP,

    // PCM control request types
    VIRTIO_SND_R_PCM_INFO = 0x0100,
    VIRTIO_SND_R_PCM_SET_PARAMS,
    VIRTIO_SND_R_PCM_PREPARE,
    VIRTIO_SND_R_PCM_RELEASE,
    VIRTIO_SND_R_PCM_START,
    VIRTIO_SND_R_PCM_STOP,

    // channel map control request types
    VIRTIO_SND_R_CHMAP_INFO = 0x0200,

    // jack event types
    VIRTIO_SND_EVT_JACK_CONNECTED = 0x1000,
    VIRTIO_SND_EVT_JACK_DISCONNECTED,

    // PCM event types
    VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED = 0x1100,
    VIRTIO_SND_EVT_PCM_XRUN,

    // common status codes
    VIRTIO_SND_S_OK = 0x8000,
    VIRTIO_SND_S_BAD_MSG,
    VIRTIO_SND_S_NOT_SUPP,
    VIRTIO_SND_S_IO_ERR
};

static const char* virtio_snd_code_str(u32 code) {
    switch (code) {
    case VIRTIO_SND_R_JACK_INFO:
        return "VIRTIO_SND_R_JACK_INFO";
    case VIRTIO_SND_R_JACK_REMAP:
        return "VIRTIO_SND_R_JACK_REMAP";

    case VIRTIO_SND_R_PCM_INFO:
        return "VIRTIO_SND_R_PCM_INFO";
    case VIRTIO_SND_R_PCM_SET_PARAMS:
        return "VIRTIO_SND_R_PCM_SET_PARAMS";
    case VIRTIO_SND_R_PCM_PREPARE:
        return "VIRTIO_SND_R_PCM_PREPARE";
    case VIRTIO_SND_R_PCM_RELEASE:
        return "VIRTIO_SND_R_PCM_RELEASE";
    case VIRTIO_SND_R_PCM_START:
        return "VIRTIO_SND_R_PCM_START";
    case VIRTIO_SND_R_PCM_STOP:
        return "VIRTIO_SND_R_PCM_STOP";

    case VIRTIO_SND_R_CHMAP_INFO:
        return "VIRTIO_SND_R_CHMAP_INFO";

    case VIRTIO_SND_EVT_JACK_CONNECTED:
        return "VIRTIO_SND_EVT_JACK_CONNECTED";
    case VIRTIO_SND_EVT_JACK_DISCONNECTED:
        return "VIRTIO_SND_EVT_JACK_DISCONNECTED";
    case VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED:
        return "VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED";
    case VIRTIO_SND_EVT_PCM_XRUN:
        return "VIRTIO_SND_EVT_PCM_XRUN";

    case VIRTIO_SND_S_OK:
        return "VIRTIO_SND_S_OK";
    case VIRTIO_SND_S_BAD_MSG:
        return "VIRTIO_SND_S_BAD_MSG";
    case VIRTIO_SND_S_NOT_SUPP:
        return "VIRTIO_SND_S_NOT_SUPP";
    case VIRTIO_SND_S_IO_ERR:
        return "VIRTIO_SND_S_IO_ERR";

    default:
        return "unknown";
    }
}

static u32 virtio_snd_get_code(vq_message& msg) {
    u32 code = 0;
    if (msg.length_in() >= sizeof(code))
        msg.copy_in(code);
    return code;
}

sound::stream_info* sound::lookup_stream(u32 streamid) {
    switch (streamid) {
    case STREAMID_TX:
        return &m_streamtx;
    case STREAMID_RX:
        return &m_streamrx;
    default:
        return nullptr;
    }
}

void sound::set_response_status(vq_message& msg, u32 resp) {
    if (msg.length_out() >= sizeof(resp))
        msg.copy_out(resp);

    if (resp != VIRTIO_SND_S_OK) {
        log_warn("command %s failed with %s",
                 virtio_snd_code_str(virtio_snd_get_code(msg)),
                 virtio_snd_code_str(resp));
    }
}

u32 sound::handle_unsupported(vq_message& msg) {
    u32 code = virtio_snd_get_code(msg);
    log_warn("unknown command code 0x%08x (%s)", code,
             virtio_snd_code_str(code));
    return VIRTIO_SND_S_NOT_SUPP;
}

u32 sound::handle_pcm_info(vq_message& msg) {
    virtio_snd_query_info hdr{};
    if (msg.length_in() != sizeof(hdr))
        return VIRTIO_SND_S_BAD_MSG;

    msg.copy_in(hdr);
    if (hdr.size < sizeof(virtio_snd_pcm_info))
        return VIRTIO_SND_S_BAD_MSG;
    if (msg.length_out() != sizeof(u32) + hdr.count * hdr.size)
        return VIRTIO_SND_S_BAD_MSG;

    for (u32 i = 0; i < hdr.count; i++) {
        virtio_snd_pcm_info info{};
        switch (hdr.start_id + i) {
        case STREAMID_TX:
            info.hdr.hda_fn_nid = 0;
            info.features = 0;
            info.formats = m_streamtx.driver_formats;
            info.rates = m_streamtx.driver_rates;
            info.direction = VIRTIO_SND_D_OUTPUT;
            info.channels_min = m_streamtx.driver_min_channels;
            info.channels_max = m_streamtx.driver_max_channels;
            break;
        case STREAMID_RX:
            info.hdr.hda_fn_nid = 0;
            info.features = 0;
            info.formats = m_streamrx.driver_formats;
            info.rates = m_streamrx.driver_rates;
            info.direction = VIRTIO_SND_D_INPUT;
            info.channels_min = m_streamrx.driver_min_channels;
            info.channels_max = m_streamrx.driver_max_channels;
            break;

        default:
            return VIRTIO_SND_S_BAD_MSG;
        }

        msg.copy_out(info, sizeof(u32) + i * hdr.size);
    }

    return VIRTIO_SND_S_OK;
}

u32 sound::handle_pcm_set_params(vq_message& msg) {
    virtio_snd_pcm_set_params req;
    if (msg.length_in() != sizeof(req))
        return VIRTIO_SND_S_BAD_MSG;

    msg.copy_in(req);

    log_debug("stream_id = %u", req.hdr.stream_id);
    log_debug("buffer_bytes = %u", req.buffer_bytes);
    log_debug("period_bytes = %u", req.period_bytes);
    log_debug("features = 0x%x", req.features);
    log_debug("channels = %hhu", req.channels);
    log_debug("format = %s", virtio_snd_pcm_format_str(req.format));
    log_debug("rate = %zu", virtio_snd_pcm_rate_hz(req.rate));

    u32 format = virtio_snd_pcm_format_to_vcml(req.format);
    u32 rate = virtio_snd_pcm_rate_hz(req.rate);

    auto* stream = lookup_stream(req.hdr.stream_id);
    if (!stream || stream->stream_id != req.hdr.stream_id)
        return VIRTIO_SND_S_BAD_MSG;

    if (req.channels < stream->driver_min_channels ||
        req.channels > stream->driver_max_channels) {
        log_warn("unsupported PCM channel count: %hhu", req.channels);
        return VIRTIO_SND_S_NOT_SUPP;
    }

    if (!stream->driver->supports_format(format)) {
        log_warn("unsupported PCM format: %hhu (%s)", req.format,
                 virtio_snd_pcm_format_str(req.format));
        return VIRTIO_SND_S_NOT_SUPP;
    }

    if (!stream->driver->supports_rate(rate)) {
        log_warn("unsupported PCM rate: %uHz", rate);
        return VIRTIO_SND_S_NOT_SUPP;
    }

    stream->channels = req.channels;
    stream->format = req.format;
    stream->rate = req.rate;
    stream->state = STATE_STOPPED;

    if (!stream->driver->configure(format, req.channels, rate)) {
        log_warn("error configuring output stream");
        return VIRTIO_SND_S_IO_ERR;
    }

    return VIRTIO_SND_S_OK;
}

u32 sound::handle_pcm_prepare(vq_message& msg) {
    virtio_snd_pcm_hdr hdr;
    if (msg.length_in() != sizeof(hdr))
        return VIRTIO_SND_S_BAD_MSG;

    msg.copy_in(hdr);
    log_debug("prepare stream %u", hdr.stream_id);
    return VIRTIO_SND_S_OK;
}

u32 sound::handle_pcm_start(vq_message& msg) {
    virtio_snd_pcm_hdr hdr;
    if (msg.length_in() != sizeof(hdr))
        return VIRTIO_SND_S_BAD_MSG;

    msg.copy_in(hdr);
    auto* stream = lookup_stream(hdr.stream_id);
    if (!stream)
        return VIRTIO_SND_S_BAD_MSG;

    log_debug("start stream %u", stream->stream_id);
    stream->state = STATE_RUNNING;
    stream->driver->start();
    stream->event->notify(SC_ZERO_TIME);
    return VIRTIO_SND_S_OK;
}

u32 sound::handle_pcm_stop(vq_message& msg) {
    virtio_snd_pcm_hdr hdr;
    if (msg.length_in() != sizeof(hdr))
        return VIRTIO_SND_S_BAD_MSG;

    msg.copy_in(hdr);
    auto* stream = lookup_stream(hdr.stream_id);
    if (!stream)
        return VIRTIO_SND_S_BAD_MSG;

    log_debug("stop stream %u", stream->stream_id);
    stream->state = STATE_STOPPED;
    stream->driver->stop();
    stream->event->notify(SC_ZERO_TIME);
    return VIRTIO_SND_S_OK;
}

u32 sound::handle_pcm_release(vq_message& msg) {
    virtio_snd_pcm_hdr hdr;
    if (msg.length_in() != sizeof(hdr))
        return VIRTIO_SND_S_BAD_MSG;

    msg.copy_in(hdr);
    auto* stream = lookup_stream(hdr.stream_id);
    if (!stream)
        return VIRTIO_SND_S_BAD_MSG;

    log_debug("release stream %u", stream->stream_id);
    if (stream->state == STATE_RUNNING)
        stream->driver->stop();
    stream->state = STATE_RELEASED;
    stream->driver->shutdown();
    stream->event->notify(SC_ZERO_TIME);
    return VIRTIO_SND_S_OK;
}

void sound::process_control(vq_message& msg) {
    u32 resp = VIRTIO_SND_S_NOT_SUPP;
    u32 code = virtio_snd_get_code(msg);
    log_debug("received command code 0x%08x (%s)", code,
              virtio_snd_code_str(code));

    switch (code) {
    case VIRTIO_SND_R_JACK_INFO:
        resp = handle_unsupported(msg);
        set_response_status(msg, resp);
        break;

    case VIRTIO_SND_R_PCM_INFO:
        resp = handle_pcm_info(msg);
        set_response_status(msg, resp);
        break;

    case VIRTIO_SND_R_PCM_SET_PARAMS:
        resp = handle_pcm_set_params(msg);
        set_response_status(msg, resp);
        break;

    case VIRTIO_SND_R_PCM_PREPARE:
        resp = handle_pcm_prepare(msg);
        set_response_status(msg, resp);
        break;

    case VIRTIO_SND_R_PCM_START:
        resp = handle_pcm_start(msg);
        set_response_status(msg, resp);
        break;

    case VIRTIO_SND_R_PCM_STOP:
        resp = handle_pcm_stop(msg);
        set_response_status(msg, resp);
        break;

    case VIRTIO_SND_R_PCM_RELEASE:
        resp = handle_pcm_release(msg);
        set_response_status(msg, resp);
        break;

    default:
        resp = handle_unsupported(msg);
        set_response_status(msg, resp);
        break;
    }
}

void sound::ctrl_thread() {
    while (true) {
        wait(m_ctrlev);
        vq_message msg;
        while (virtio_in->get(VIRTQUEUE_CONTROL, msg)) {
            process_control(msg);
            if (!virtio_in->put(VIRTQUEUE_CONTROL, msg))
                break;
        }
    }
}

void sound::tx_thread() {
    while (true) {
        while (m_streamtx.state == STATE_STOPPED)
            wait(m_txev);

        while (m_streamtx.state != STATE_STOPPED) {
            vq_message msg;
            if (!virtio_in->get(VIRTQUEUE_TX, msg)) {
                m_streamtx.state = STATE_STOPPED;
                break;
            }

            if (m_streamtx.state == STATE_RUNNING) {
                virtio_snd_pcm_xfer hdr{};
                virtio_snd_pcm_status sts{};
                msg.copy_in(hdr);

                size_t buflen = msg.length_in() - sizeof(hdr);
                sc_time delay = virtio_snd_buffer_duration(
                    buflen, m_streamtx.format, m_streamtx.channels,
                    m_streamtx.rate);

                vector<u8> buf(buflen);
                msg.copy_in(buf, sizeof(hdr));
                m_output.xfer(buf);

                wait(delay);

                sts.status = VIRTIO_SND_S_OK;
                sts.latency_bytes = buflen;
                msg.copy_out(sts);
            }

            virtio_in->put(VIRTQUEUE_TX, msg);
        }
    }
}

void sound::rx_thread() {
    while (true) {
        while (m_streamrx.state == STATE_STOPPED)
            wait(m_rxev);

        while (m_streamrx.state != STATE_STOPPED) {
            vq_message msg;
            if (!virtio_in->get(VIRTQUEUE_RX, msg)) {
                m_streamrx.state = STATE_STOPPED;
                break;
            }

            if (m_streamrx.state == STATE_RUNNING) {
                virtio_snd_pcm_xfer hdr{};
                virtio_snd_pcm_status sts{};
                msg.copy_in(hdr);

                size_t buflen = msg.length_out() - sizeof(sts);
                sc_time delay = virtio_snd_buffer_duration(
                    buflen, m_streamrx.format, m_streamrx.channels,
                    m_streamrx.rate);
                wait(delay);

                vector<u8> buf(buflen);
                m_input.xfer(buf);
                msg.copy_out(buf);

                sts.status = VIRTIO_SND_S_OK;
                sts.latency_bytes = buflen;
                msg.copy_out(sts, buflen);
            }

            virtio_in->put(VIRTQUEUE_RX, msg);
        }
    }
}

void sound::identify(virtio_device_desc& desc) {
    reset();
    desc.device_id = VIRTIO_DEVICE_SOUND;
    desc.vendor_id = VIRTIO_VENDOR_VCML;
    desc.pci_class = PCI_CLASS_MULTIMEDIA_AUDIO;
    desc.request_virtqueue(VIRTQUEUE_CONTROL, 64);
    desc.request_virtqueue(VIRTQUEUE_EVENT, 64);
    desc.request_virtqueue(VIRTQUEUE_TX, 64);
    desc.request_virtqueue(VIRTQUEUE_RX, 64);
}

bool sound::notify(u32 vqid) {
    switch (vqid) {
    case VIRTQUEUE_CONTROL:
        m_ctrlev.notify(SC_ZERO_TIME);
        break;

    case VIRTQUEUE_EVENT:
        log_debug("event queue currently unused");
        break;

    case VIRTQUEUE_TX:
        m_txev.notify(SC_ZERO_TIME);
        break;

    case VIRTQUEUE_RX:
        m_rxev.notify(SC_ZERO_TIME);
        break;

    default:
        log_warn("unknown virtqueue %u", vqid);
    }

    return true;
}

void sound::read_features(u64& features) {
    features = 0;
}

bool sound::write_features(u64 features) {
    return true;
}

bool sound::read_config(const range& addr, void* ptr) {
    if (addr.end >= sizeof(m_config))
        return false;

    memcpy(ptr, (u8*)&m_config + addr.start, addr.length());
    return true;
}

bool sound::write_config(const range& addr, const void* ptr) {
    return false;
}

sound::sound(const sc_module_name& nm):
    module(nm),
    virtio_device(),
    m_config(),
    m_streamtx(),
    m_streamrx(),
    m_input("input"),
    m_output("output"),
    m_ctrlev("ctrlev"),
    m_txev("txev"),
    m_rxev("rxev"),
    virtio_in("virtio_in") {
    m_config.jacks = 0;
    m_config.streams = STREAMID_NUM;
    m_config.chmaps = 0;

    m_streamtx.stream_id = STREAMID_TX;
    m_streamtx.driver_formats = virtio_snd_supported_formats(m_output);
    m_streamtx.driver_rates = virtio_snd_supported_rates(m_output);
    m_streamtx.driver_min_channels = m_output.min_channels();
    m_streamtx.driver_max_channels = m_output.max_channels();
    m_streamtx.driver = &m_output;
    m_streamtx.event = &m_txev;

    m_streamrx.stream_id = STREAMID_RX;
    m_streamrx.driver_formats = virtio_snd_supported_formats(m_input);
    m_streamrx.driver_rates = virtio_snd_supported_rates(m_input);
    m_streamrx.driver_min_channels = m_input.min_channels();
    m_streamrx.driver_max_channels = m_input.max_channels();
    m_streamrx.driver = &m_input;
    m_streamrx.event = &m_rxev;

    SC_HAS_PROCESS(sound);
    SC_THREAD(ctrl_thread);
    SC_THREAD(tx_thread);
    SC_THREAD(rx_thread);
}

sound::~sound() {
    // nothing to do
}

void sound::reset() {
    m_streamtx.state = STATE_STOPPED;
    m_streamrx.state = STATE_STOPPED;
}

VCML_EXPORT_MODEL(vcml::virtio::sound, name, args) {
    return new sound(name);
}

} // namespace virtio
} // namespace vcml
