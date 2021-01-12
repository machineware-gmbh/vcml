/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VIRTIO_INPUT_H
#define VCML_VIRTIO_INPUT_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/ui/keymap.h"
#include "vcml/ui/display.h"

#include "vcml/range.h"
#include "vcml/module.h"

#include "vcml/properties/property.h"
#include "vcml/models/virtio/virtio.h"

namespace vcml { namespace virtio {

    class input : public module,
                  private virtio_fw_transport_if
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
            VIRTIO_INPUT_CFG_UNSET     = 0x00,
            VIRTIO_INPUT_CFG_ID_NAME   = 0x01,
            VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
            VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
            VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
            VIRTIO_INPUT_CFG_EV_BITS   = 0x11,
            VIRTIO_INPUT_CFG_ABS_INFO  = 0x12,
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
                u8   bitmap[128];
                input_absinfo abs;
                input_devids  ids;
            } u;
        };

        input_config m_config;

        ui::key_listener m_key_listener;
        ui::ptr_listener m_ptr_listener;

        bool m_shift;
        bool m_capsl;
        bool m_alt_l;
        bool m_alt_r;

        u32 m_prev_symbol;
        u32 m_prev_btn;
        u32 m_prev_x;
        u32 m_prev_y;

        mutex m_events_mutex;
        queue<input_event> m_events;
        queue<vq_message> m_messages;

        void push_key(u16 key, u32 down) {
            m_events.push({EV_KEY, key, down});
        }

        void push_abs(u16 axis, u32 val) {
            m_events.push({EV_ABS, axis, val});
        }

        void push_sync() {
            m_events.push({EV_SYN, SYN_REPORT, 0});
        }

        void config_update_name();
        void config_update_serial();
        void config_update_devids();
        void config_update_props();
        void config_update_evbits();
        void config_update_absinfo();

        void config_update();

        void key_event(u32 code, bool state);
        void ptr_event(u32 buttons, u32 x, u32 y);

        void update();

        virtual void identify(virtio_device_desc& desc) override;
        virtual bool notify(u32 vqid) override;

        virtual void read_features(u64& features) override;
        virtual bool write_features(u64 features) override;

        virtual bool read_config(const range& addr, void* ptr) override;
        virtual bool write_config(const range& addr, const void* ptr) override;

    public:
        property<bool> touchpad;
        property<bool> keyboard;

        property<u64> pollrate;

        property<string> keymap;
        property<string> display;

        virtio_target_socket VIRTIO_IN;

        input(const sc_module_name& nm);
        virtual ~input();
        VCML_KIND(virtio::input);

        virtual void reset();

    protected:
        virtual void end_of_simulation() override;
    };

}}

#endif
