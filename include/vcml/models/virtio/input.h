/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_INPUT_H
#define VCML_VIRTIO_INPUT_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/display.h"
#include "vcml/ui/console.h"

#include "vcml/properties/property.h"
#include "vcml/protocols/virtio.h"

namespace vcml {
namespace virtio {

class input : public module, public virtio_device
{
private:
    enum virtqueues : int {
        VIRTQUEUE_EVENT = 0,
        VIRTQUEUE_STATUS = 1,
    };

    struct input_event {
        u16 type;
        u16 code;
        u32 value;
    };

    enum config_select : u8 {
        VIRTIO_INPUT_CFG_UNSET = 0x00,
        VIRTIO_INPUT_CFG_ID_NAME = 0x01,
        VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
        VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
        VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
        VIRTIO_INPUT_CFG_EV_BITS = 0x11,
        VIRTIO_INPUT_CFG_ABS_INFO = 0x12,
    };

    struct input_absinfo {
        u32 min;
        u32 max;
        u32 fuzz;
        u32 flat;
        u32 res;
    };

    struct input_devids {
        u16 bustype;
        u16 vendor;
        u16 product;
        u16 version;
    };

    struct input_config {
        u8 select;
        u8 subsel;
        u8 size;
        u8 reserved[5];
        union {
            char string[128];
            u8 bitmap[128];
            input_absinfo abs;
            input_devids ids;
        } u;
    };

    input_config m_config;

    ui::keyboard m_keyboard;
    ui::pointer m_pointer;
    ui::console m_console;

    queue<input_event> m_events;
    queue<vq_message> m_messages;

    void push_key(u16 key, u32 down) {
        m_events.push({ ui::EV_KEY, key, down });
    }

    void push_rel(u16 axis, u32 val) {
        m_events.push({ ui::EV_REL, axis, val });
    }

    void push_abs(u16 axis, u32 val) {
        m_events.push({ ui::EV_ABS, axis, val });
    }

    void push_sync() { m_events.push({ ui::EV_SYN, ui::SYN_REPORT, 0 }); }

    void config_update_name();
    void config_update_serial();
    void config_update_devids();
    void config_update_props();
    void config_update_evbits();
    void config_update_absinfo();

    void config_update();

    void update();

    virtual void identify(virtio_device_desc& desc) override;
    virtual bool notify(u32 vqid) override;

    virtual void read_features(u64& features) override;
    virtual bool write_features(u64 features) override;

    virtual bool read_config(const range& addr, void* ptr) override;
    virtual bool write_config(const range& addr, const void* ptr) override;

public:
    const property<bool> touchpad;
    const property<bool> keyboard;
    const property<bool> mouse;

    const property<u64> pollrate;

    const property<string> keymap;

    const property<size_t> xmax;
    const property<size_t> ymax;

    virtio_target_socket virtio_in;

    input(const sc_module_name& nm);
    virtual ~input();
    VCML_KIND(virtio::input);

    virtual void reset();

protected:
    virtual void end_of_simulation() override;
};

} // namespace virtio
} // namespace vcml

#endif
