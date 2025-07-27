/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_SOUND_H
#define VCML_VIRTIO_SOUND_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/audio/istream.h"
#include "vcml/audio/ostream.h"

#include "vcml/protocols/virtio.h"

namespace vcml {
namespace virtio {

class sound : public module, public virtio_device
{
private:
    enum virtqueues : int {
        VIRTQUEUE_CONTROL = 0,
        VIRTQUEUE_EVENT,
        VIRTQUEUE_TX,
        VIRTQUEUE_RX,
    };

    struct sound_config {
        u32 jacks;
        u32 streams;
        u32 chmaps;
    };

    enum stream_state {
        STATE_STOPPED,
        STATE_RUNNING,
        STATE_RELEASED,
    };

    struct stream_info {
        u32 stream_id;
        u32 format;
        u32 rate;
        u32 channels;
        stream_state state;
        u64 driver_formats;
        u64 driver_rates;
        u32 driver_min_channels;
        u32 driver_max_channels;
        audio::stream* driver;
        sc_event* event;
    };

    sound_config m_config;

    stream_info m_streamtx;
    stream_info m_streamrx;

    audio::istream m_input;
    audio::ostream m_output;

    sc_event m_ctrlev;
    sc_event m_txev;
    sc_event m_rxev;

    stream_info* lookup_stream(u32 streamid);

    void set_response_status(vq_message& msg, u32 resp);

    u32 handle_unsupported(vq_message& msg);
    u32 handle_pcm_info(vq_message& msg);
    u32 handle_pcm_set_params(vq_message& msg);
    u32 handle_pcm_prepare(vq_message& msg);
    u32 handle_pcm_start(vq_message& msg);
    u32 handle_pcm_stop(vq_message& msg);
    u32 handle_pcm_release(vq_message& msg);

    void process_control(vq_message& msg);

    void ctrl_thread();
    void tx_thread();
    void rx_thread();

    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

public:
    virtio_target_socket virtio_in;

    sound(const sc_module_name& nm);
    virtual ~sound();
    VCML_KIND(virtio::sound);

    virtual void reset() override;
};

} // namespace virtio
} // namespace vcml

#endif
