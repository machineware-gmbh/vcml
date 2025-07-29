/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_HEADSET_H
#define VCML_USB_HEADSET_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/audio/istream.h"
#include "vcml/audio/ostream.h"

#include "vcml/protocols/usb.h"
#include "vcml/models/usb/device.h"

namespace vcml {
namespace usb {

class headset : public device
{
private:
    bool m_input_muted;
    bool m_output_muted;

    audio::istream m_input;
    audio::ostream m_output;

public:
    property<u16> vendorid;
    property<u16> productid;

    property<string> manufacturer;
    property<string> product;
    property<string> serialno;
    property<string> keymap;

    usb_target_socket usb_in;

    headset(const sc_module_name& nm);
    virtual ~headset();
    VCML_KIND(usb::headset);

protected:
    usb_result get_audio_attribute(u8 req, u8 control, u8 channel, u8 ifx,
                                   u8 entity, u8* data, size_t length);
    usb_result set_audio_attribute(u8 req, u8 control, u8 channel, u8 ifx,
                                   u8 entity, u8* data, size_t length);

    usb_result setup_playback_interface(u8 altsetting);
    usb_result setup_capture_interface(u8 altsetting);

    virtual usb_result switch_interface(size_t idx,
                                        const interface_desc& ifx) override;

    virtual usb_result get_data(u32 ep, u8* data, size_t len) override;
    virtual usb_result set_data(u32 ep, const u8* data, size_t len) override;

    virtual usb_result handle_control(u16 req, u16 val, u16 idx, u8* data,
                                      size_t length) override;
};

} // namespace usb
} // namespace vcml

#endif
