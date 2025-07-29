/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/headset.h"
#include "vcml/core/version.h"

namespace vcml {
namespace usb {

enum audio_strings : u8 {
    STRID_AUDIO_CONFIG = 8,
    STRID_AUDIO_PLAYBACK,
    STRID_AUDIO_PLAYBACK_OFF,
    STRID_AUDIO_CAPTURE,
    STRID_AUDIO_CAPTURE_OFF,
};

enum audio_interfaces : u8 {
    IFXID_CONTROL = 0,
    IFXID_PLAYBACK = 1,
    IFXID_CAPTURE = 2,
};

enum audio_entities : u8 {
    ENTID_PLAYBACK_INPUT = 1,
    ENTID_PLAYBACK_FEATURE = 2,
    ENTID_PLAYBACK_OUTPUT = 3,
    ENTID_CAPTURE_INPUT = 4,
    ENTID_CAPTURE_FEATURE = 5,
    ENTID_CAPTURE_OUTPUT = 6,
};

enum audio_interface_settings : u8 {
    ALTSET_PLAYBACK_OFF = 0,
    ALTSET_PLAYBACK_ON = 1,
    ALTSET_CAPTURE_OFF = 0,
    ALTSET_CAPTURE_ON = 1,
};

enum audio_endpoints : u8 {
    EPID_PLAYBACK = 1,
    EPID_CAPTURE = 2,
};

enum usb_subclass_audio : u8 {
    USB_SUBCLASS_CONTROL = 1,
    USB_SUBCLASS_STREAMING = 2,
};

enum usb_subtype_uac : u8 {
    USB_DST_AC_HEADER = 1,
    USB_DST_AC_INPUT_TERMINAL = 2,
    USB_DST_AC_OUTPUT_TERMINAL = 3,
    USB_DST_AC_FEATURE_UNIT = 6,
    USB_DST_AS_GENERAL = 1,
    USB_DST_AS_FORMAT_TYPE = 2,
    USB_DST_EP_GENERAL = 1,
};

enum usb_audio_control : u8 {
    USB_AUDIO_MUTE_CONTROL = 0x01,
    USB_AUDIO_VOLUME_CONTROL = 0x02,
};

constexpr u32 audio_control(u8 entity, u8 control) {
    return (u32)entity << 8 | control;
}

const char* usb_audio_control_str(u8 entity) {
    switch (entity) {
    case USB_AUDIO_MUTE_CONTROL:
        return "MUTE_CONTROL";
    case USB_AUDIO_VOLUME_CONTROL:
        return "VOLUME_CONTROL";
    default:
        return "UKNOWN_CONTROL";
    }
}

enum audio_requests : u8 {
    USB_AUDIO_REQ_SET_CUR = 0x01,
    USB_AUDIO_REQ_SET_MIN = 0x02,
    USB_AUDIO_REQ_SET_MAX = 0x03,
    USB_AUDIO_REQ_SET_RES = 0x04,
    USB_AUDIO_REQ_GET_CUR = 0x81,
    USB_AUDIO_REQ_GET_MIN = 0x82,
    USB_AUDIO_REQ_GET_MAX = 0x83,
    USB_AUDIO_REQ_GET_RES = 0x84,
};

const char* usb_audio_request_str(u8 request) {
    switch (request) {
    case USB_AUDIO_REQ_SET_CUR:
        return "REQ_SET_CUR";
    case USB_AUDIO_REQ_SET_MIN:
        return "REQ_SET_MIN";
    case USB_AUDIO_REQ_SET_MAX:
        return "REQ_SET_MAX";
    case USB_AUDIO_REQ_SET_RES:
        return "REQ_SET_RES";
    case USB_AUDIO_REQ_GET_CUR:
        return "REQ_GET_CUR";
    case USB_AUDIO_REQ_GET_MIN:
        return "REQ_GET_MIN";
    case USB_AUDIO_REQ_GET_MAX:
        return "REQ_GET_MAX";
    case USB_AUDIO_REQ_GET_RES:
        return "REQ_GET_RES";
    default:
        return "REQ_UNKNOWN";
    }
}

static const device_desc HEADSET_DESC{
    USB_2_0,            // bcd_usb
    0,                  // device class
    0,                  // device subclass
    0,                  // device protocol
    64,                 // max packet size
    0xffff,             // vendor id (patched later)
    0xffff,             // device id (patched later)
    0x0,                // bcd_device
    "MachineWare GmbH", // vendor string
    "VCML Headset",     // product string
    VCML_GIT_REV_SHORT, // serial number string
    {
        {
            // configurations
            1,                                   // bConfigurationValue
            USB_CFG_ONE | USB_CFG_REMOTE_WAKEUP, // bmAttributes
            250,                                 // bMaxPower (500mA)
            {
                // interface 0
                {
                    IFXID_CONTROL,        // bInterfaceNumber
                    0,                    // bAlternateSetting
                    USB_CLASS_AUDIO,      // bInterfaceClass
                    USB_SUBCLASS_CONTROL, // bInterfaceSubClass
                    0,                    // bInterfaceProtocol
                    STRID_AUDIO_CONFIG,   // iInterface
                    {},                   // no endpoints
                    {
                        // extra descriptors
                        {
                            // UAC interface header descriptor
                            0x0a,                // bLength
                            USB_DT_CS_INTERFACE, // bDescriptorType
                            USB_DST_AC_HEADER,   // bDescriptorSubType
                            0x00,                // bcdADC[0]
                            0x01,                // bcdADC[1]
                            0x45,                // wTotalLength[0]
                            0x00,                // wTotalLength[1]
                            0x02,                // bInCollection
                            IFXID_PLAYBACK,      // bInterfaceNr
                            IFXID_CAPTURE,       // bInterfaceNr

                            // UAC interface input terminal descriptor
                            0x0c,                      // bLength
                            USB_DT_CS_INTERFACE,       // bDescriptorType
                            USB_DST_AC_INPUT_TERMINAL, // bDescriptorSubType
                            ENTID_PLAYBACK_INPUT,      // bTerminalID
                            0x01,                      // wTerminalType[0]
                            0x01,                      // wTerminalType[1]
                            0x00,                      // bAssocTerminal
                            0x02,                      // bNrChannels
                            0x03,                      // wChannelConfig[0]
                            0x00,                      // wChannelConfig[1]
                            0x00,                      // iChannelNames
                            0x00,                      // iTerminal

                            // UAC interface feature unit descriptor
                            0x09,
                            USB_DT_CS_INTERFACE,     // bDescriptorType
                            USB_DST_AC_FEATURE_UNIT, // bDescriptorSubType
                            ENTID_PLAYBACK_FEATURE,  // bUnitID
                            ENTID_PLAYBACK_INPUT,    // bSourceID
                            0x01,                    // bControlSize
                            bit(USB_AUDIO_MUTE_CONTROL - 1), // bmaControls[0]
                            bit(USB_AUDIO_MUTE_CONTROL - 1), // bmaControls[1]
                            0x00,                            // iFeature

                            // UAC interface output terminal descriptor
                            0x09,                       // bLength
                            USB_DT_CS_INTERFACE,        // bDescriptorType
                            USB_DST_AC_OUTPUT_TERMINAL, // bDescriptorSubType
                            ENTID_PLAYBACK_OUTPUT,      // bTerminalID
                            0x02,                       // wTerminalType[0]
                            0x03,                       // wTerminalType[1]
                            ENTID_PLAYBACK_INPUT,       // bAssocTerminal
                            ENTID_PLAYBACK_FEATURE,     // bSourceID
                            0x00,                       // iTerminal

                            // UAC interface input terminal descriptor
                            0x0c,                      // bLength
                            USB_DT_CS_INTERFACE,       // bDescriptorType
                            USB_DST_AC_INPUT_TERMINAL, // bDescriptorSubType
                            ENTID_CAPTURE_INPUT,       // bTerminalID
                            0x01,                      // wTerminalType[0]
                            0x02,                      // wTerminalType[1]
                            0x00,                      // bAssocTerminal
                            0x01,                      // bNrChannels
                            0x00,                      // wChannelConfig[0]
                            0x00,                      // wChannelConfig[1]
                            0x00,                      // iChannelNames
                            0x00,                      // iTerminal

                            // UAC interface feature unit descriptor
                            0x08,
                            USB_DT_CS_INTERFACE,     // bDescriptorType
                            USB_DST_AC_FEATURE_UNIT, // bDescriptorSubType
                            ENTID_CAPTURE_FEATURE,   // bUnitID
                            ENTID_CAPTURE_INPUT,     // bSourceID
                            0x01,                    // bControlSize
                            bit(USB_AUDIO_MUTE_CONTROL - 1), // bmaControls[0]
                            0x00,                            // iFeature

                            // UAC interface output terminal descriptor
                            0x09,                       // bLength
                            USB_DT_CS_INTERFACE,        // bDescriptorType
                            USB_DST_AC_OUTPUT_TERMINAL, // bDescriptorSubType
                            ENTID_PLAYBACK_OUTPUT,      // bTerminalID
                            0x01,                       // wTerminalType[0]
                            0x01,                       // wTerminalType[1]
                            ENTID_CAPTURE_INPUT,        // bAssocTerminal
                            ENTID_CAPTURE_FEATURE,      // bSourceID
                            0x00,                       // iTerminal
                        },
                    },
                },

                // interface 1 (altsetting 0 = off)
                {
                    IFXID_PLAYBACK,          // bInterfaceNumber
                    ALTSET_PLAYBACK_OFF,     // bAlternateSetting
                    USB_CLASS_AUDIO,         // bInterfaceClass
                    USB_SUBCLASS_STREAMING,  // bInterfaceSubClass
                    0,                       // bInterfaceProtocol
                    STRID_AUDIO_CAPTURE_OFF, // iInterface
                    {},                      // no endpoints
                    {},                      // no extra descriptors
                },

                // interface 1 (altsetting 1 = stereo output)
                {
                    IFXID_PLAYBACK,         // bInterfaceNumber
                    ALTSET_PLAYBACK_ON,     // bAlternateSetting
                    USB_CLASS_AUDIO,        // bInterfaceClass
                    USB_SUBCLASS_STREAMING, // bInterfaceSubClass
                    0,                      // bInterfaceProtocol
                    STRID_AUDIO_CAPTURE,    // iInterface
                    {
                        // endpoints
                        {
                            usb_ep_out(EPID_PLAYBACK),      // bEndpointAddress
                            USB_EP_ISOC | USB_EP_ISOC_SYNC, // bmAttributes
                            2 * 96,                         // wMaxPacketSize
                            1,                              // bInterval
                            true,                           // is_audio
                            0,                              // bRefresh
                            0,                              // bSynchAddress
                            {
                                // extra
                                0x07,               // bLength
                                USB_DT_CS_ENDPOINT, // bDescriptorType
                                USB_DST_EP_GENERAL, // bDescriptorSubType
                                0x00,               // bmAttributes
                                0x00,               // bLockDelayUnits
                                0x00,               // wLockDelay[0]
                                0x00,               // wLockDelay[1]
                            },
                        },
                    },

                    // extra descriptors
                    {
                        // AS General descriptor
                        0x07,                // bLength
                        USB_DT_CS_INTERFACE, // bDescriptorType
                        USB_DST_AS_GENERAL,  // bDescriptorSubType
                        0x01,                // bTerminalLink
                        0x00,                // bDelay
                        0x01,                // wFormatTag[0]
                        0x00,                // wFormatTag[1]

                        // Format Type descriptor
                        0x0b,                   // bLength
                        USB_DT_CS_INTERFACE,    // bDescriptorType
                        USB_DST_AS_FORMAT_TYPE, // bDescriptorSubType
                        0x01,                   // bFormatType
                        0x02,                   // bNrChannels
                        0x02,                   // bSubFrameSize
                        0x10,                   // bBitResolution
                        0x01,                   // bSamFreqType
                        0x80,                   // tSamFreq[0]
                        0xbb,                   // tSamFreq[1]
                        0x00,                   // tSamFreq[2]
                    },
                },

                // interface 2 (altsetting 0 = off)
                {
                    IFXID_CAPTURE,           // bInterfaceNumber
                    ALTSET_CAPTURE_OFF,      // bAlternateSetting
                    USB_CLASS_AUDIO,         // bInterfaceClass
                    USB_SUBCLASS_STREAMING,  // bInterfaceSubClass
                    0,                       // bInterfaceProtocol
                    STRID_AUDIO_CAPTURE_OFF, // iInterface
                    {},                      // no endpoints
                    {},                      // no extra descriptors
                },

                // interface 2 (altsetting 1 = mono recording)
                {
                    IFXID_CAPTURE,           // bInterfaceNumber
                    ALTSET_CAPTURE_ON,       // bAlternateSetting
                    USB_CLASS_AUDIO,         // bInterfaceClass
                    USB_SUBCLASS_STREAMING,  // bInterfaceSubClass
                    0,                       // bInterfaceProtocol
                    STRID_AUDIO_CAPTURE_OFF, // iInterface
                    {
                        // endpoints
                        {
                            usb_ep_in(EPID_CAPTURE),        // bEndpointAddress
                            USB_EP_ISOC | USB_EP_ISOC_SYNC, // bmAttributes
                            1 * 96,                         // wMaxPacketSize
                            1,                              // bInterval
                            true,                           // is_audio
                            0,                              // bRefresh
                            0,                              // bSynchAddress
                            {
                                // extra
                                0x07,               // bLength
                                USB_DT_CS_ENDPOINT, // bDescriptorType
                                USB_DST_EP_GENERAL, // bDescriptorSubType
                                0x00,               // bmAttributes
                                0x00,               // bLockDelayUnits
                                0x00,               // wLockDelay[0]
                                0x00,               // wLockDelay[1]
                            },
                        },
                    },

                    // extra descriptors
                    {
                        // AS General descriptor
                        0x07,                // bLength
                        USB_DT_CS_INTERFACE, // bDescriptorType
                        USB_DST_AS_GENERAL,  // bDescriptorSubType
                        0x01,                // bTerminalLink
                        0x00,                // bDelay
                        0x01,                // wFormatTag[0]
                        0x00,                // wFormatTag[1]

                        // Format Type descriptor
                        0x0b,                   // bLength
                        USB_DT_CS_INTERFACE,    // bDescriptorType
                        USB_DST_AS_FORMAT_TYPE, // bDescriptorSubType
                        0x01,                   // bFormatType
                        0x01,                   // bNrChannels
                        0x02,                   // bSubFrameSize
                        0x10,                   // bBitResolution
                        0x01,                   // bSamFreqType
                        0x80,                   // tSamFreq[0]
                        0xbb,                   // tSamFreq[1]
                        0x00,                   // tSamFreq[2]
                    },
                },
            },
        },
    },
};

headset::headset(const sc_module_name& nm):
    device(nm, HEADSET_DESC),
    m_input_muted(),
    m_output_muted(),
    m_input("input"),
    m_output("output"),
    vendorid("vendorid", USB_VENDOR_VCML),
    productid("productid", 0x1),
    manufacturer("manufacturer", "MachineWare GmbH"),
    product("product", "VCML headset"),
    serialno("serialno", VCML_GIT_REV_SHORT),
    keymap("keymap", "us"),
    usb_in("usb_in") {
    m_desc.vendor_id = vendorid;
    m_desc.product_id = productid;
    m_desc.manufacturer = manufacturer;
    m_desc.product = product;
    m_desc.serial_number = serialno;

    define_string(STRID_AUDIO_CONFIG, "Audio Configuration");
    define_string(STRID_AUDIO_PLAYBACK, "Audio Playback - 48kHz Stereo");
    define_string(STRID_AUDIO_PLAYBACK_OFF, "Audio Playback - Off");
    define_string(STRID_AUDIO_CAPTURE, "Audio Capture - 48kHz Mono");
    define_string(STRID_AUDIO_CAPTURE_OFF, "Audio Capture - Off");
}

headset::~headset() {
    // nothing to do
}

usb_result headset::get_audio_attribute(u8 req, u8 control, u8 channel, u8 ifx,
                                        u8 entity, u8* data, size_t length) {
    if (ifx != IFXID_CONTROL) {
        log_warn("%s: invalid interface %hhu", __func__, ifx);
        return USB_RESULT_STALL;
    }

    if (length < 1) {
        log_warn("%s: insufficient data", __func__);
        return USB_RESULT_STALL;
    }

    switch (audio_control(entity, control)) {
    case audio_control(ENTID_CAPTURE_FEATURE, USB_AUDIO_MUTE_CONTROL):
        data[0] = m_input_muted;
        return USB_RESULT_SUCCESS;

    case audio_control(ENTID_PLAYBACK_FEATURE, USB_AUDIO_MUTE_CONTROL):
        data[0] = m_output_muted;
        return USB_RESULT_SUCCESS;

    default:
        log_warn("%s: invalid control %hhu or entity 0x%hhx", __func__,
                 control, entity);
        return USB_RESULT_STALL;
    }
}

usb_result headset::set_audio_attribute(u8 req, u8 control, u8 channel, u8 ifx,
                                        u8 entity, u8* data, size_t length) {
    if (ifx != IFXID_CONTROL) {
        log_warn("%s: invalid interface %hhu", __func__, ifx);
        return USB_RESULT_STALL;
    }

    if (length < 1) {
        log_warn("%s: insufficient data", __func__);
        return USB_RESULT_STALL;
    }

    switch (audio_control(entity, control)) {
    case audio_control(ENTID_CAPTURE_FEATURE, USB_AUDIO_MUTE_CONTROL):
        if (m_input_muted ^ !!data[0])
            log_debug("input %smuted", m_input_muted ? "un" : "");
        m_input_muted = !!data[0];
        return USB_RESULT_SUCCESS;

    case audio_control(ENTID_PLAYBACK_FEATURE, USB_AUDIO_MUTE_CONTROL):
        if (m_output_muted ^ !!data[0])
            log_debug("output %smuted", m_output_muted ? "un" : "");
        m_output_muted = !!data[0];
        return USB_RESULT_SUCCESS;

    default:
        log_warn("%s: invalid control %hhu or entity 0x%hhx", __func__,
                 control, entity);
        return USB_RESULT_STALL;
    }
}

usb_result headset::setup_playback_interface(u8 altsetting) {
    switch (altsetting) {
    case ALTSET_PLAYBACK_OFF:
        m_output.stop();
        m_output.shutdown();
        return USB_RESULT_SUCCESS;
    case ALTSET_PLAYBACK_ON:
        m_output.configure(audio::FORMAT_S16LE, 2, 48000);
        m_output.start();
        return USB_RESULT_SUCCESS;
    default:
        log_warn("unknown playback altsetting: %hhu", altsetting);
        return USB_RESULT_STALL;
    }
}

usb_result headset::setup_capture_interface(u8 altsetting) {
    switch (altsetting) {
    case ALTSET_CAPTURE_ON:
        m_input.configure(audio::FORMAT_S16LE, 2, 48000);
        m_input.start();
        return USB_RESULT_SUCCESS;
    case ALTSET_CAPTURE_OFF:
        m_input.stop();
        m_input.shutdown();
        return USB_RESULT_SUCCESS;
    default:
        log_warn("unknown capture altsetting: %hhu", altsetting);
        return USB_RESULT_STALL;
    }
}

usb_result headset::switch_interface(size_t idx, const interface_desc& ifx) {
    switch (idx) {
    case IFXID_PLAYBACK:
        return setup_playback_interface(ifx.alternate_setting);

    case IFXID_CAPTURE:
        return setup_capture_interface(ifx.alternate_setting);

    default:
        log_warn("unknown audio interface: %zu", idx);
        return USB_RESULT_STALL;
    }
}

usb_result headset::get_data(u32 ep, u8* data, size_t len) {
    if (ep != EPID_CAPTURE) {
        log_warn("invalid audio capture endpoint: %u", ep);
        return USB_RESULT_STALL;
    }

    if (!m_input_muted && altset_active(IFXID_CAPTURE, ALTSET_CAPTURE_ON))
        m_input.xfer(data, len);
    else
        audio::fill_silence(data, len, audio::FORMAT_S16LE);

    return USB_RESULT_SUCCESS;
}

usb_result headset::set_data(u32 ep, const u8* data, size_t len) {
    if (ep != EPID_PLAYBACK) {
        log_warn("invalid audio playback endpoint: %u", ep);
        return USB_RESULT_STALL;
    }

    if (!m_output_muted && altset_active(IFXID_PLAYBACK, ALTSET_PLAYBACK_ON))
        m_output.xfer(data, len);

    return USB_RESULT_SUCCESS;
}

usb_result headset::handle_control(u16 req, u16 val, u16 idx, u8* data,
                                   size_t length) {
    switch (req & 0xff80) {
    case USB_REQ_IN | USB_REQ_CLASS | USB_REQ_IFACE | 0x80: {
        return get_audio_attribute(req & 0xff, val >> 8, val & 0xff,
                                   idx & 0xff, idx >> 8, data, length);
    }

    case USB_REQ_OUT | USB_REQ_CLASS | USB_REQ_IFACE: {
        return set_audio_attribute(req & 0xff, val >> 8, val & 0xff,
                                   idx & 0xff, idx >> 8, data, length);
    }

    default:
        return device::handle_control(req, val, idx, data, length);
    }
}

VCML_EXPORT_MODEL(vcml::usb::headset, name, args) {
    return new headset(name);
}

} // namespace usb
} // namespace vcml
