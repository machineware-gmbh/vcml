/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "usb_test.h"

enum test_requests : u16 {
    REQ_VENDOR_IN = USB_REQ_IN | USB_REQ_VENDOR | USB_REQ_DEVICE | 0x42,
    REQ_VENDOR_OUT = USB_REQ_OUT | USB_REQ_VENDOR | USB_REQ_DEVICE | 0x43,
};

static const vector<u8> DEVICE_DESC = {
    0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x37,
    0x13, 0x34, 0x12, 0x02, 0x01, 0x01, 0x02, 0x03, 0x02,
};

static const vector<u8> CONFIG_A = {
    0x09, 0x02, 0x69, 0x00, 0x02, 0x07, 0x00, 0xe0, 0x32, // config
    0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0x01, 0x02, 0x04, // ifx 0.0
    0x05, 0x24, 0x01, 0x02, 0x03,                         // extra
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x0a,             // ep 0x81
    0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00,             // ep 0x02
    0x04, 0x25, 0xaa, 0xbb,                               // extra
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, // ifx 1.0
    0x09, 0x04, 0x01, 0x01, 0x02, 0x01, 0x02, 0x00, 0x05, // ifx 1.1
    0x03, 0x24, 0x01,                                     // extra
    0x09, 0x05, 0x03, 0x09, 0xc0, 0x00, 0x01, 0x00, 0x83, // ep 0x03
    0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,             // extra
    0x09, 0x05, 0x83, 0x11, 0x03, 0x00, 0x01, 0x05, 0x00, // ep 0x83
    0x09, 0x04, 0x01, 0x02, 0x01, 0x01, 0x02, 0x00, 0x00, // ifx 1.2
    0x09, 0x05, 0x03, 0x05, 0x80, 0x01, 0x01, 0x00, 0x00, // ep 0x03
};

static const vector<u8> CONFIG_B = {
    0x09, 0x02, 0x19, 0x00, 0x01, 0x09, 0x00, 0x80, 0x64, // config
    0x09, 0x04, 0x00, 0x00, 0x01, 0x08, 0x06, 0x50, 0x00, // ifx 0.0
    0x07, 0x05, 0x85, 0x02, 0x00, 0x02, 0x00,             // ep 0x85
};

static usb::device_desc make_desc(u16 bcd) {
    usb::device_desc desc{};
    desc.bcd_usb = bcd;
    desc.max_packet_size0 = 64;
    desc.vendor_id = USB_VENDOR_VCML;
    desc.product_id = 0x1234;
    desc.bcd_device = 0x0102;
    desc.manufacturer = "MachineWare";
    desc.product = "Test Device";
    desc.serial_number = "SN42";

    usb::config_desc a{};
    a.value = 7;
    a.attributes = USB_CFG_ONE | USB_CFG_SELF_POWERED | USB_CFG_REMOTE_WAKEUP;
    a.max_power = 50;
    a.interfaces = {
        { 0,
          0,
          USB_CLASS_VENDOR_SPECIFIC,
          1,
          2,
          4,
          {
              { usb_ep_in(1), USB_EP_IRQ, 8, 10, 0, 0, 0, {} },
              { usb_ep_out(2),
                USB_EP_BULK,
                512,
                0,
                0,
                0,
                0,
                { 4, USB_DT_CS_ENDPOINT, 0xaa, 0xbb } },
          },
          { 5, USB_DT_CS_INTERFACE, 1, 2, 3 } },
        { 1, 0, USB_CLASS_AUDIO, 2, 0, 0, {}, {} },
        { 1,
          1,
          USB_CLASS_AUDIO,
          2,
          0,
          5,
          {
              { usb_ep_out(3),
                USB_EP_ISOC | USB_EP_ISOC_ADAPT,
                192,
                1,
                1,
                0,
                usb_ep_in(3),
                { 7, USB_DT_CS_ENDPOINT, 1, 0, 0, 0, 0 } },
              { usb_ep_in(3),
                USB_EP_ISOC | USB_EP_ISOC_FEEDBACK,
                3,
                1,
                1,
                5,
                0,
                {} },
          },
          { 3, USB_DT_CS_INTERFACE, 1 } },
        { 1,
          2,
          USB_CLASS_AUDIO,
          2,
          0,
          0,
          {
              { usb_ep_out(3),
                USB_EP_ISOC | USB_EP_ISOC_ASYNC,
                384,
                1,
                1,
                0,
                0,
                {} },
          },
          {} },
    };

    usb::config_desc b{};
    b.value = 9;
    b.attributes = USB_CFG_ONE;
    b.max_power = 100;
    b.interfaces = {
        { 0,
          0,
          USB_CLASS_MASS_STORAGE,
          6,
          0x50,
          0,
          { { usb_ep_in(5), USB_EP_BULK, 512, 0, 0, 0, 0, {} } },
          {} },
    };

    desc.configs = { a, b };
    return desc;
}

class test_device : public usb::device
{
public:
    usb_target_socket usb_in;
    usb_target_socket usb_aux;
    usb_target_array<> usb_arr;

    vector<u16> requests;
    vector<u8> vendor_data;
    vector<u8> configs;
    vector<pair<size_t, u8>> interfaces;
    vector<u32> in_eps;
    vector<u32> out_eps;
    vector<u8> out_data;
    vector<int> reset_eps;
    size_t resets;

    usb_result config_result;
    usb_result ifx_result;

    test_device(const sc_module_name& nm, u16 bcd):
        usb::device(nm, make_desc(bcd)),
        usb_in("usb_in"),
        usb_aux("usb_aux"),
        usb_arr("usb_arr"),
        requests(),
        vendor_data(),
        configs(),
        interfaces(),
        in_eps(),
        out_eps(),
        out_data(),
        reset_eps(),
        resets(0),
        config_result(USB_RESULT_SUCCESS),
        ifx_result(USB_RESULT_SUCCESS) {
        usb_arr.get(0);
        define_string(4, "Interface");
        define_string(5, "Other");
    }

    usb::device_desc& desc() { return m_desc; }

    bool active(u32 ifx, u32 altset) const {
        return altset_active(ifx, altset);
    }

    void define(u8 idx, const string& str) { define_string(idx, str); }

    vector<usb_target_socket*> sockets() { return all_usb_sockets(); }

    usb_target_socket* find(const string& name) {
        return find_usb_socket(name);
    }

    usb_target_socket* find(const string& name, size_t idx) {
        return find_usb_socket(name, idx);
    }

protected:
    virtual usb_result get_data(u32 ep, u8* data, size_t len) override {
        in_eps.push_back(ep);
        if (ep == 1) {
            for (size_t i = 0; i < len; i++)
                data[i] = (u8)(0xa0 + i);
            return USB_RESULT_SUCCESS;
        }

        if (ep == 4)
            return USB_RESULT_NACK;

        return usb::device::get_data(ep, data, len);
    }

    virtual usb_result set_data(u32 ep, const u8* data, size_t len) override {
        out_eps.push_back(ep);
        if (ep == 2) {
            out_data.assign(data, data + len);
            return USB_RESULT_SUCCESS;
        }

        return usb::device::set_data(ep, data, len);
    }

    virtual usb_result switch_configuration(
        const usb::config_desc& cfg) override {
        configs.push_back(cfg.value);
        if (failed(config_result))
            return config_result;
        return usb::device::switch_configuration(cfg);
    }

    virtual usb_result switch_interface(
        size_t idx, const usb::interface_desc& ifx) override {
        interfaces.emplace_back(idx, ifx.alternate_setting);
        if (failed(ifx_result))
            return ifx_result;
        return usb::device::switch_interface(idx, ifx);
    }

    virtual usb_result handle_control(u16 req, u16 val, u16 idx, u8* data,
                                      size_t length) override {
        requests.push_back(req);
        switch (req) {
        case REQ_VENDOR_IN:
            for (size_t i = 0; i < length; i++)
                data[i] = (u8)(i ^ val ^ idx);
            return USB_RESULT_SUCCESS;

        case REQ_VENDOR_OUT:
            vendor_data.assign(data, data + length);
            return USB_RESULT_SUCCESS;

        default:
            return usb::device::handle_control(req, val, idx, data, length);
        }
    }

    virtual void usb_reset_device() override {
        resets++;
        usb::device::usb_reset_device();
    }

    virtual void usb_reset_endpoint(int ep) override {
        reset_eps.push_back(ep);
        usb::device::usb_reset_endpoint(ep);
    }
};

class plain_device : public module, public usb_dev_if
{
public:
    usb_target_socket usb_in;

    plain_device(const sc_module_name& nm):
        module(nm), usb_dev_if(), usb_in("usb_in") {}
};

class usb_device_test : public test_base, public usb_host_if
{
public:
    usb_stim stim;
    usb_stim stim3;
    test_device dev;
    test_device dev3;
    plain_device plain;

    usb_initiator_socket host_out;
    usb_initiator_socket plain_out;
    usb_initiator_socket stub_out;

    usb_device_test(const sc_module_name& nm):
        test_base(nm),
        usb_host_if(),
        stim("stim"),
        stim3("stim3"),
        dev("dev", USB_2_0),
        dev3("dev3", USB_3_0),
        plain("plain"),
        host_out("host_out"),
        plain_out("plain_out"),
        stub_out("stub_out") {
        stim.usb_out.bind(dev.usb_in);
        usb_stub(dev, "usb_aux");
        usb_stub(dev, "usb_arr", 0);

        stim3.usb_out.bind(dev3.usb_in);
        host_out.bind(dev3.usb_aux);
        usb_stub(dev3, "usb_arr", 0);

        plain_out.bind(plain.usb_in);
        usb_stub(*this, "stub_out");

        add_test("strings", &usb_device_test::test_strings);
        add_test("attach", &usb_device_test::test_attach);
        add_test("sockets", &usb_device_test::test_sockets);
        add_test("descriptors", &usb_device_test::test_descriptors);
        add_test("address", &usb_device_test::test_address);
        add_test("configuration", &usb_device_test::test_configuration);
        add_test("interface", &usb_device_test::test_interface);
        add_test("requests", &usb_device_test::test_requests);
        add_test("ep0", &usb_device_test::test_ep0);
        add_test("data", &usb_device_test::test_data);
    }

    void reset_host() {
        stim.usb_out.reset_device();
        stim.addr = 0;
        stim.packet_size = 64;
    }

    usb_result get_configuration(u8& cfg) {
        vector<u8> data;
        usb_result res = stim.control_in(
            USB_REQ_DEVICE | USB_REQ_GET_CONFIGURATION, 0, 0, data, 1);
        cfg = data[0];
        return res;
    }

    usb_result get_interface(u16 ifx, u8& alt) {
        vector<u8> data;
        usb_result res = stim.control_in(USB_REQ_IFACE | USB_REQ_GET_INTERFACE,
                                         0, ifx, data, 1);
        alt = data[0];
        return res;
    }

    usb_result get_status(vector<u8>& data, u16 val = 0) {
        return stim.control_in(USB_REQ_DEVICE | USB_REQ_GET_STATUS, val, 0,
                               data, 2);
    }

    void test_strings() {
        EXPECT_STREQ(dev.kind(), "vcml::usb::device");
        EXPECT_STREQ(dev.version(), VCML_VERSION_STRING);
    }

    void test_attach() {
        EXPECT_EQ(stim.attached, 1u);
        EXPECT_EQ(stim.detached, 0u);
        EXPECT_TRUE(stim.usb_out.is_attached());
        EXPECT_EQ(stim.usb_out.connection_speed(), USB_SPEED_HIGH);
        EXPECT_EQ(dev.usb_in.connection_speed(), USB_SPEED_HIGH);
        EXPECT_TRUE(dev.usb_aux.is_attached());
        EXPECT_TRUE(dev.usb_arr[0].is_attached());

        EXPECT_FALSE(dev3.start_attached);
        EXPECT_EQ(stim3.attached, 0u);
        EXPECT_FALSE(dev3.usb_in.is_attached());
        EXPECT_FALSE(host_out.is_attached());

        vector<u8> data;
        EXPECT_EQ(stim3.in(1, data, 8), USB_RESULT_NACK);

        std::stringstream ss;
        EXPECT_TRUE(dev3.execute("usb_attach", ss));
        EXPECT_NE(
            ss.str().find("attaching test.dev3.usb_in (USB_SPEED_SUPER)"),
            string::npos);
        EXPECT_EQ(stim3.attached, 1u);
        EXPECT_EQ(stim3.usb_out.connection_speed(), USB_SPEED_SUPER);
        EXPECT_EQ(host_out.connection_speed(), USB_SPEED_SUPER);
        EXPECT_TRUE(dev3.usb_arr[0].is_attached());

        EXPECT_TRUE(dev3.execute("usb_attach", ss));
        EXPECT_EQ(stim3.attached, 1u);
        EXPECT_EQ(stim3.detached, 0u);

        ss.str("");
        EXPECT_TRUE(dev3.execute("usb_detach", { "usb_in" }, ss));
        EXPECT_NE(ss.str().find("detaching test.dev3.usb_in"), string::npos);
        EXPECT_EQ(stim3.detached, 1u);
        EXPECT_FALSE(stim3.usb_out.is_attached());
        EXPECT_TRUE(host_out.is_attached());

        EXPECT_TRUE(dev3.execute("usb_detach", { "usb_in" }, ss));
        EXPECT_EQ(stim3.detached, 1u);

        ss.str("");
        EXPECT_FALSE(dev3.execute("usb_attach", { "nope" }, ss));
        EXPECT_EQ(ss.str(), "usb socket not found: nope");
        ss.str("");
        EXPECT_FALSE(dev3.execute("usb_detach", { "usb_in", "nope" }, ss));
        EXPECT_EQ(ss.str(), "usb socket not found: nope");
        EXPECT_EQ(stim3.detached, 1u);

        EXPECT_TRUE(dev3.execute("usb_attach", { "usb_in" }, ss));
        EXPECT_EQ(stim3.attached, 2u);
        EXPECT_EQ(stim3.usb_out.connection_speed(), USB_SPEED_SUPER);

        dev3.usb_in.attach(USB_SPEED_HIGH);
        EXPECT_EQ(stim3.detached, 2u);
        EXPECT_EQ(stim3.attached, 3u);
        EXPECT_EQ(stim3.usb_out.connection_speed(), USB_SPEED_HIGH);

        EXPECT_TRUE(dev3.execute("usb_detach", ss));
        EXPECT_EQ(stim3.detached, 3u);
        EXPECT_FALSE(host_out.is_attached());
        EXPECT_FALSE(dev3.usb_arr[0].is_attached());

        dev3.desc().bcd_usb = 0x0099;
        ss.str("");
        EXPECT_FALSE(dev3.execute("usb_attach", ss));
        EXPECT_NE(ss.str().find("cannot get usb speed"), string::npos);
        EXPECT_FALSE(stim3.usb_out.is_attached());

        dev3.desc().bcd_usb = USB_2_1;
        EXPECT_TRUE(dev3.execute("usb_attach", { "usb_in" }, ss));
        EXPECT_EQ(stim3.usb_out.connection_speed(), USB_SPEED_HIGH);
        EXPECT_TRUE(dev3.execute("usb_detach", ss));
        dev3.desc().bcd_usb = USB_3_0;
        EXPECT_EQ(stim3.attached, 4u);
        EXPECT_EQ(stim3.detached, 4u);
    }

    void test_sockets() {
        auto all = dev.sockets();
        EXPECT_EQ(all.size(), 3u);
        EXPECT_TRUE(stl_contains(all, &dev.usb_in));
        EXPECT_TRUE(stl_contains(all, &dev.usb_aux));
        EXPECT_TRUE(stl_contains(all, &dev.usb_arr[0]));

        EXPECT_EQ(dev.find("usb_in"), &dev.usb_in);
        EXPECT_EQ(dev.find("usb_aux"), &dev.usb_aux);
        EXPECT_EQ(dev.find("nope"), nullptr);
        EXPECT_EQ(dev.find("usb_arr", 0), &dev.usb_arr[0]);
        EXPECT_EQ(dev.find("usb_arr", 1), nullptr);
        EXPECT_EQ(dev.find("usb_in", 0), nullptr);
        EXPECT_EQ(dev.find("nope", 0), nullptr);

        u8 buf[4] = { 1, 2, 3, 4 };
        plain.usb_in.attach(USB_SPEED_FULL);
        EXPECT_TRUE(plain_out.is_attached());
        usb_packet p = usb_packet_out(0, 1, buf, sizeof(buf));
        plain_out.send(p);
        EXPECT_EQ(p.result, USB_RESULT_INCOMPLETE);
        plain_out.reset_device();
        plain_out.reset_endpoint(1);
        plain.usb_in.detach();
        EXPECT_FALSE(plain_out.is_attached());

        p = usb_packet_in(0, 1, buf, sizeof(buf));
        stub_out.send(p);
        EXPECT_EQ(p.result, USB_RESULT_INCOMPLETE);
        stub_out.reset_device();
        stub_out.reset_endpoint(2);
        EXPECT_FALSE(stub_out.is_attached());
    }

    void test_descriptors() {
        reset_host();

        vector<u8> data;
        ASSERT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, DEVICE_DESC);

        usb_device_desc dd;
        memcpy(&dd, data.data(), sizeof(dd));
        EXPECT_EQ(dd.length, sizeof(usb_device_desc));
        EXPECT_EQ(dd.bcd_usb, USB_2_0);
        EXPECT_EQ(dd.num_configurations, 2);

        ASSERT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 8),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data,
                  vector<u8>(DEVICE_DESC.begin(), DEVICE_DESC.begin() + 8));

        ASSERT_EQ(stim.get_descriptor(USB_DT_CONFIG, 0, data, 9),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>(CONFIG_A.begin(), CONFIG_A.begin() + 9));

        usb_config_desc cd;
        memcpy(&cd, data.data(), sizeof(cd));
        EXPECT_EQ(cd.length, sizeof(usb_config_desc));
        EXPECT_EQ(cd.descriptor_type, USB_DT_CONFIG);
        EXPECT_EQ(cd.total_length, CONFIG_A.size());
        EXPECT_EQ(cd.num_interfaces, 2);
        EXPECT_EQ(cd.configuration_value, 7);

        ASSERT_EQ(stim.get_descriptor(USB_DT_CONFIG, 0, data, cd.total_length),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, CONFIG_A);

        size_t pos = 0, nifx = 0, nep = 0, nextra = 0;
        while (pos < data.size()) {
            ASSERT_GE(data[pos], 2);
            switch (data[pos + 1]) {
            case USB_DT_INTERFACE:
                EXPECT_EQ(data[pos], sizeof(usb_interface_desc));
                nifx++;
                break;
            case USB_DT_ENDPOINT:
                nep++;
                break;
            default:
                nextra++;
                break;
            }

            pos += data[pos];
        }

        EXPECT_EQ(pos, data.size());
        EXPECT_EQ(nifx, 4u);
        EXPECT_EQ(nep, 5u);
        EXPECT_EQ(nextra, 5u);

        ASSERT_EQ(stim.get_descriptor(USB_DT_CONFIG, 1, data, 255),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(vector<u8>(data.begin(), data.begin() + CONFIG_B.size()),
                  CONFIG_B);
        EXPECT_EQ(stim.get_descriptor(USB_DT_CONFIG, 2, data, 9),
                  USB_RESULT_STALL);

        ASSERT_EQ(stim.get_descriptor(USB_DT_CONFIG, 0, data, 0xffff),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(vector<u8>(data.begin(), data.begin() + CONFIG_A.size()),
                  CONFIG_A);

        ASSERT_EQ(stim.get_descriptor(USB_DT_STRING, 0, data, 4),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 4, USB_DT_STRING, 0x09, 0x04 }));

        ASSERT_EQ(stim.get_descriptor(USB_DT_STRING, 3, data, 10),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 10, USB_DT_STRING, 'S', 0, 'N', 0, '4', 0,
                                     '2', 0 }));

        EXPECT_EQ(stim.get_string(1), "MachineWare");
        EXPECT_EQ(stim.get_string(2), "Test Device");
        EXPECT_EQ(stim.get_string(3), "SN42");
        EXPECT_EQ(stim.get_string(4), "Interface");
        EXPECT_EQ(stim.get_string(5), "Other");
        EXPECT_EQ(stim.get_string(9), "???");

        dev.define(9, "Nine");
        EXPECT_EQ(stim.get_string(9), "Nine");
        EXPECT_EQ(stim.get_string(4), "Interface");

        ASSERT_EQ(stim.get_descriptor(USB_DT_DEVICE_QUALIFIER, 0, data, 10),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 10, USB_DT_DEVICE_QUALIFIER, 0x00, 0x02,
                                     0, 0, 0, 64, 2, 0 }));

        ASSERT_EQ(stim.get_descriptor(USB_DT_BOS, 0, data, 5),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 5, USB_DT_BOS, 5, 0, 0 }));

        EXPECT_EQ(stim.get_descriptor(USB_DT_DEBUG, 0, data, 4),
                  USB_RESULT_STALL);
        EXPECT_EQ(stim.get_descriptor(USB_DT_OTHER_SPEED_CONFIG, 0, data, 9),
                  USB_RESULT_STALL);
        EXPECT_EQ(stim.get_descriptor(0x42, 0, data, 9), USB_RESULT_STALL);
        EXPECT_EQ(
            stim.get_descriptor(USB_DT_DEVICE, 0, data, 18, USB_REQ_IFACE),
            USB_RESULT_STALL);

        ASSERT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, DEVICE_DESC);
    }

    void test_address() {
        reset_host();
        EXPECT_FALSE(dev.is_addressed());
        EXPECT_EQ(dev.get_address(), 0u);

        vector<u8> data;
        stim.addr = 17;
        EXPECT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        stim.addr = 0;

        ASSERT_EQ(stim.set_address(5), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.addr, 5u);
        EXPECT_TRUE(dev.is_addressed());
        EXPECT_FALSE(dev.is_configured());
        EXPECT_EQ(dev.get_address(), 5u);

        stim.addr = 0;
        EXPECT_EQ(
            stim.setup(USB_REQ_IN | USB_REQ_GET_DESCRIPTOR, 0x0100, 0, 18),
            USB_RESULT_NODEV);
        EXPECT_EQ(stim.in(0, data, 8), USB_RESULT_NODEV);
        EXPECT_EQ(stim.out(1, data), USB_RESULT_NODEV);
        stim.addr = 6;
        EXPECT_EQ(stim.in(1, data, 8), USB_RESULT_NODEV);
        stim.addr = 5;

        EXPECT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, DEVICE_DESC);

        u8 cfg = 0xff;
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 0);

        size_t resets = dev.resets;
        stim.usb_out.reset_device();
        EXPECT_EQ(dev.resets, resets + 1);
        EXPECT_FALSE(dev.is_addressed());
        EXPECT_EQ(dev.get_address(), 0u);

        stim.addr = 0;
        EXPECT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
    }

    void test_configuration() {
        reset_host();
        dev.configs.clear();

        EXPECT_EQ(stim.set_configuration(7), USB_RESULT_STALL);
        EXPECT_FALSE(dev.is_configured());
        EXPECT_TRUE(dev.configs.empty());

        ASSERT_EQ(stim.set_address(1), USB_RESULT_SUCCESS);

        u8 cfg = 0xff;
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 0);

        vector<u8> data;
        EXPECT_EQ(get_status(data), USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 0, 0 }));

        EXPECT_EQ(stim.set_configuration(8), USB_RESULT_STALL);
        EXPECT_FALSE(dev.is_configured());
        EXPECT_TRUE(dev.configs.empty());

        EXPECT_EQ(stim.set_configuration(7), USB_RESULT_SUCCESS);
        EXPECT_TRUE(dev.is_configured());
        EXPECT_EQ(dev.configs, vector<u8>({ 7 }));
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 7);

        EXPECT_EQ(get_status(data), USB_RESULT_SUCCESS);
        EXPECT_EQ(data[0] & 1, 1);
        EXPECT_EQ(data[1], 0);
        EXPECT_EQ(get_status(data, 1), USB_RESULT_STALL);

        EXPECT_EQ(stim.set_configuration(9), USB_RESULT_SUCCESS);
        EXPECT_EQ(dev.configs, vector<u8>({ 7, 9 }));
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 9);
        EXPECT_EQ(get_status(data), USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 0, 0 }));

        EXPECT_EQ(stim.set_configuration(0), USB_RESULT_SUCCESS);
        EXPECT_FALSE(dev.is_configured());
        EXPECT_TRUE(dev.is_addressed());
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 0);
        EXPECT_EQ(dev.configs, vector<u8>({ 7, 9 }));

        dev.config_result = USB_RESULT_STALL;
        EXPECT_EQ(stim.set_configuration(9), USB_RESULT_STALL);
        dev.config_result = USB_RESULT_SUCCESS;

        EXPECT_EQ(stim.set_configuration(7), USB_RESULT_SUCCESS);
        EXPECT_TRUE(dev.is_configured());

        reset_host();
        EXPECT_FALSE(dev.is_configured());
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 0);
        EXPECT_EQ(get_status(data), USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 0, 0 }));
    }

    void test_interface() {
        reset_host();
        ASSERT_EQ(stim.set_address(2), USB_RESULT_SUCCESS);

        u8 alt = 0xff;
        EXPECT_EQ(get_interface(0, alt), USB_RESULT_STALL);
        EXPECT_EQ(stim.set_interface(0, 0), USB_RESULT_STALL);
        EXPECT_FALSE(dev.active(0, 0));

        ASSERT_EQ(stim.set_configuration(7), USB_RESULT_SUCCESS);
        EXPECT_EQ(get_interface(0, alt), USB_RESULT_SUCCESS);
        EXPECT_EQ(alt, 0);
        EXPECT_EQ(get_interface(1, alt), USB_RESULT_SUCCESS);
        EXPECT_EQ(alt, 0);
        EXPECT_TRUE(dev.active(0, 0));
        EXPECT_TRUE(dev.active(1, 0));
        EXPECT_FALSE(dev.active(1, 1));
        EXPECT_FALSE(dev.active(2, 0));

        EXPECT_EQ(get_interface(2, alt), USB_RESULT_STALL);
        EXPECT_EQ(get_interface(7, alt), USB_RESULT_STALL);

        dev.interfaces.clear();
        EXPECT_EQ(stim.set_interface(1, 1), USB_RESULT_SUCCESS);
        ASSERT_EQ(dev.interfaces.size(), 1u);
        EXPECT_EQ(dev.interfaces[0].first, 1u);
        EXPECT_EQ(dev.interfaces[0].second, 1);
        EXPECT_EQ(get_interface(1, alt), USB_RESULT_SUCCESS);
        EXPECT_EQ(alt, 1);
        EXPECT_TRUE(dev.active(1, 1));
        EXPECT_FALSE(dev.active(1, 0));
        EXPECT_TRUE(dev.active(0, 0));

        EXPECT_EQ(stim.set_interface(1, 3), USB_RESULT_STALL);
        EXPECT_EQ(stim.set_interface(2, 0), USB_RESULT_STALL);
        EXPECT_EQ(stim.set_interface(9, 0), USB_RESULT_STALL);
        EXPECT_EQ(dev.interfaces.size(), 1u);
        EXPECT_EQ(get_interface(1, alt), USB_RESULT_SUCCESS);
        EXPECT_EQ(alt, 1);

        EXPECT_EQ(stim.set_interface(1, 2), USB_RESULT_SUCCESS);
        EXPECT_EQ(get_interface(1, alt), USB_RESULT_SUCCESS);
        EXPECT_EQ(alt, 2);
        EXPECT_TRUE(dev.active(1, 2));

        dev.ifx_result = USB_RESULT_STALL;
        EXPECT_EQ(stim.set_interface(1, 0), USB_RESULT_STALL);
        dev.ifx_result = USB_RESULT_SUCCESS;

        ASSERT_EQ(stim.set_configuration(7), USB_RESULT_SUCCESS);
        EXPECT_TRUE(dev.active(1, 0));
        EXPECT_FALSE(dev.active(1, 2));
        EXPECT_EQ(get_interface(1, alt), USB_RESULT_SUCCESS);
        EXPECT_EQ(alt, 0);

        ASSERT_EQ(stim.set_configuration(9), USB_RESULT_SUCCESS);
        EXPECT_TRUE(dev.active(0, 0));
        EXPECT_FALSE(dev.active(1, 0));
        EXPECT_EQ(get_interface(1, alt), USB_RESULT_STALL);

        ASSERT_EQ(stim.set_configuration(0), USB_RESULT_SUCCESS);
        EXPECT_FALSE(dev.active(0, 0));
        EXPECT_EQ(get_interface(0, alt), USB_RESULT_STALL);

        EXPECT_EQ(
            stim.control_out(USB_REQ_DEVICE | USB_REQ_SET_ISOCH_DELAY, 40, 0),
            USB_RESULT_SUCCESS);
    }

    void test_requests() {
        reset_host();

        vector<u8> data;
        dev.requests.clear();
        ASSERT_EQ(stim.control_in(REQ_VENDOR_IN, 0x12, 0x34, data, 100),
                  USB_RESULT_SUCCESS);
        ASSERT_EQ(data.size(), 100u);
        for (size_t i = 0; i < data.size(); i++) {
            EXPECT_EQ(data[i], (u8)(i ^ 0x12 ^ 0x34)) << "at index " << i;
        }
        EXPECT_EQ(dev.requests, vector<u16>({ REQ_VENDOR_IN }));

        vector<u8> payload(150);
        for (size_t i = 0; i < payload.size(); i++)
            payload[i] = (u8)(3 * i);
        ASSERT_EQ(stim.control_out(REQ_VENDOR_OUT, 1, 2, payload),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(dev.vendor_data, payload);

        ASSERT_EQ(stim.control_out(REQ_VENDOR_OUT, 1, 2), USB_RESULT_SUCCESS);
        EXPECT_TRUE(dev.vendor_data.empty());

        {
            mock_publisher pub(LOG_ERROR, LOG_ERROR);
            EXPECT_CALL(pub, publish(_)).Times(AnyNumber());
            pub.expect(LOG_ERROR, "unknown vendor device rd request 0xc044");
            pub.expect(LOG_ERROR, "unknown class iface wr request 0x2101");
            pub.expect(LOG_ERROR, "unknown endpoint rd request 0x820c");

            EXPECT_EQ(stim.control_in(USB_REQ_VENDOR | 0x44, 0, 0, data, 4),
                      USB_RESULT_STALL);
            EXPECT_EQ(
                stim.control_out(USB_REQ_CLASS | USB_REQ_IFACE | 1, 0, 0),
                USB_RESULT_STALL);
            EXPECT_EQ(stim.control_in(USB_REQ_ENDPOINT | USB_REQ_SYNCH_FRAME,
                                      0, usb_ep_in(1), data, 2),
                      USB_RESULT_STALL);
        }

        dev.requests.clear();
        stim.control_in(USB_REQ_IFACE | USB_REQ_GET_STATUS, 0, 0, data, 2);
        stim.control_in(USB_REQ_ENDPOINT | USB_REQ_GET_STATUS, 0, usb_ep_in(1),
                        data, 2);
        stim.control_out(USB_REQ_ENDPOINT | USB_REQ_CLEAR_FEATURE, 0,
                         usb_ep_in(1));
        stim.control_out(USB_REQ_DEVICE | USB_REQ_SET_FEATURE, 1, 0);
        EXPECT_EQ(dev.requests,
                  vector<u16>({ 0x8100, 0x8200, 0x0201, 0x0003 }));

        EXPECT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, DEVICE_DESC);
    }

    void test_ep0() {
        reset_host();

        u8 buf[64] = {};
        usb_packet p = usb_packet_setup(0, buf, 7);
        stim.usb_out.send(p);
        EXPECT_EQ(p.result, USB_RESULT_STALL);
        EXPECT_EQ(stim.in(0, buf, 8), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(0, nullptr, 0), USB_RESULT_STALL);

        vector<u8> data;
        stim.packet_size = 8;
        ASSERT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, DEVICE_DESC);

        vector<u8> payload(20);
        for (size_t i = 0; i < payload.size(); i++)
            payload[i] = (u8)(0x80 + i);
        ASSERT_EQ(stim.control_out(REQ_VENDOR_OUT, 0, 0, payload),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(dev.vendor_data, payload);
        stim.packet_size = 64;

        EXPECT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 0),
                  USB_RESULT_SUCCESS);

        u16 rd = USB_REQ_IN | USB_REQ_GET_DESCRIPTOR;
        ASSERT_EQ(stim.setup(rd, 0x0100, 0, 18), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.out(0, buf, 8), USB_RESULT_STALL);
        EXPECT_EQ(stim.in(0, buf, 8), USB_RESULT_STALL);

        ASSERT_EQ(stim.setup(REQ_VENDOR_OUT, 0, 0, 4), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.in(0, buf, 4), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(0, buf, 4), USB_RESULT_STALL);

        EXPECT_EQ(stim.get_descriptor(USB_DT_DEBUG, 0, data, 4),
                  USB_RESULT_STALL);
        EXPECT_EQ(stim.in(0, buf, 4), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(0, nullptr, 0), USB_RESULT_STALL);
        ASSERT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);
        EXPECT_EQ(data, DEVICE_DESC);

        {
            mock_publisher pub(LOG_ERROR, LOG_ERROR);
            pub.expect(LOG_ERROR, "received input token without prior setup");
            pub.expect(LOG_ERROR, "USB_TOKEN_OUT: not in data state");
            stim.in(0, buf, 8);
            stim.out(0, buf, 8);
        }

        {
            mock_publisher pub(LOG_ERROR, LOG_ERROR);
            pub.expect(LOG_ERROR, "invalid usb data token received");
            p = usb_packet_in(0, 1, buf, 8);
            p.token = USB_TOKEN_SETUP;
            stim.usb_out.send(p);
            EXPECT_EQ(p.result, USB_RESULT_STALL);
        }

        ASSERT_EQ(stim.setup(rd, 0x0100, 0, 18), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.in(0, buf, 8), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.in(4, buf + 8, 8), USB_RESULT_NACK);
        EXPECT_EQ(stim.in(6, buf + 8, 8), USB_RESULT_STALL);
        EXPECT_EQ(stim.in(0, buf + 8, 10), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.out(0, nullptr, 0), USB_RESULT_SUCCESS);
        EXPECT_EQ(vector<u8>(buf, buf + 18), DEVICE_DESC);
    }

    void test_data() {
        reset_host();
        ASSERT_EQ(stim.set_address(3), USB_RESULT_SUCCESS);
        ASSERT_EQ(stim.set_configuration(7), USB_RESULT_SUCCESS);

        vector<u8> data;
        dev.in_eps.clear();
        dev.out_eps.clear();
        EXPECT_EQ(stim.in(1, data, 8), USB_RESULT_SUCCESS);
        EXPECT_EQ(data, vector<u8>({ 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
                                     0xa7 }));

        vector<u8> payload = { 1, 2, 3, 4, 5 };
        EXPECT_EQ(stim.out(2, payload), USB_RESULT_SUCCESS);
        EXPECT_EQ(dev.out_data, payload);

        EXPECT_EQ(stim.in(4, data, 8), USB_RESULT_NACK);
        EXPECT_EQ(stim.in(6, data, 4), USB_RESULT_STALL);
        EXPECT_EQ(stim.in(6, data, 0), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.out(6, payload), USB_RESULT_STALL);
        EXPECT_EQ(stim.out(6, nullptr, 0), USB_RESULT_SUCCESS);
        EXPECT_EQ(dev.in_eps, vector<u32>({ 1, 4, 6, 6 }));
        EXPECT_EQ(dev.out_eps, vector<u32>({ 2, 6, 6 }));

        EXPECT_EQ(stim.in(1, data, 8), USB_RESULT_SUCCESS);
        EXPECT_EQ(stim.get_descriptor(USB_DT_DEVICE, 0, data, 18),
                  USB_RESULT_SUCCESS);

        dev.reset_eps.clear();
        stim.usb_out.reset_endpoint(1);
        EXPECT_EQ(dev.reset_eps, vector<int>({ 1 }));
        EXPECT_TRUE(dev.is_configured());

        u8 buf[8] = {};
        ASSERT_EQ(
            stim.setup(USB_REQ_IN | USB_REQ_GET_DESCRIPTOR, 0x0100, 0, 18),
            USB_RESULT_SUCCESS);
        stim.usb_out.reset_endpoint(0);
        EXPECT_EQ(dev.reset_eps, vector<int>({ 1, 0 }));
        EXPECT_TRUE(dev.is_configured());
        EXPECT_EQ(dev.get_address(), 3u);
        EXPECT_TRUE(dev.active(0, 0));

        {
            mock_publisher pub(LOG_ERROR, LOG_ERROR);
            pub.expect(LOG_ERROR, "received input token without prior setup");
            stim.in(0, buf, 8);
        }

        u8 cfg = 0;
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 7);

        reset_host();
        EXPECT_FALSE(dev.is_addressed());
        EXPECT_FALSE(dev.is_configured());
        EXPECT_EQ(dev.get_address(), 0u);
        EXPECT_FALSE(dev.active(0, 0));
        EXPECT_EQ(get_configuration(cfg), USB_RESULT_SUCCESS);
        EXPECT_EQ(cfg, 0);
    }
};

TEST(usb_device, simulate) {
    broker broker("test");
    broker.define("test.dev3.start_attached", false);

    usb_device_test test("test");
    sc_core::sc_start();
}
