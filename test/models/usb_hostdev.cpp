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

class log_capture : public mwr::publisher
{
private:
    mutable mutex m_mtx;
    vector<string> m_lines;

public:
    log_capture(): mwr::publisher(LOG_ERROR, LOG_DEBUG), m_mtx(), m_lines() {}
    virtual ~log_capture() = default;

    virtual void publish(const mwr::logmsg& msg) override {
        lock_guard<mutex> guard(m_mtx);
        for (const string& line : msg.lines)
            m_lines.push_back(line);
    }

    size_t count(const string& text) const {
        lock_guard<mutex> guard(m_mtx);
        size_t n = 0;
        for (const string& line : m_lines) {
            if (line.find(text) != string::npos)
                n++;
        }
        return n;
    }
};

class hostdev_test : public test_base
{
public:
    log_capture logs;

    usb_stim stim;
    usb::hostdev unconfigured;
    usb::hostdev nodev;
    usb::hostdev nodev_props;

    hostdev_test(const sc_module_name& nm):
        test_base(nm),
        logs(),
        stim("stim"),
        unconfigured("unconfigured"),
        nodev("nodev", 255, 255),
        nodev_props("nodev_props") {
        stim.usb_out.bind(unconfigured.usb_in);
        usb_stub(nodev, "usb_in");
        usb_stub(nodev_props, "usb_in");
        add_test("test_strings", &hostdev_test::test_strings);
        add_test("test_properties", &hostdev_test::test_properties);
        add_test("test_detached", &hostdev_test::test_detached);
        add_test("test_commands", &hostdev_test::test_commands);
        add_test("test_packets", &hostdev_test::test_packets);
        add_test("test_reset", &hostdev_test::test_reset);
    }

    string execute(usb::hostdev& dev, const string& cmd,
                   const vector<string>& args, bool expect) {
        stringstream ss;
        EXPECT_EQ(dev.execute(cmd, args, ss), expect) << cmd;
        return ss.str();
    }

    usb_result send_packet(usb::hostdev& dev, usb_packet& p) {
        usb_dev_if& dif = dev;
        dif.usb_transport(p);
        return p.result;
    }

    usb_result setup(usb::hostdev& dev, u16 req, u16 val, u16 idx, u16 len) {
        u8 data[8] = {
            (u8)(req >> 8), (u8)req,        (u8)val, (u8)(val >> 8),
            (u8)idx,        (u8)(idx >> 8), (u8)len, (u8)(len >> 8),
        };

        usb_packet p = usb_packet_setup(0, data, sizeof(data));
        return send_packet(dev, p);
    }

    usb_result in(usb::hostdev& dev, u32 ep, size_t len) {
        vector<u8> data(len);
        usb_packet p = usb_packet_in(0, ep, data.data(), len);
        return send_packet(dev, p);
    }

    usb_result out(usb::hostdev& dev, u32 ep, size_t len) {
        vector<u8> data(len, 0x5a);
        usb_packet p = usb_packet_out(0, ep, data.data(), len);
        return send_packet(dev, p);
    }

    void test_strings() {
        EXPECT_STREQ(unconfigured.kind(), "vcml::usb::hostdev");
        EXPECT_STREQ(unconfigured.version(), VCML_VERSION_STRING);
    }

    void test_properties() {
        EXPECT_EQ(unconfigured.hostbus.get(), 0u);
        EXPECT_EQ(unconfigured.hostaddr.get(), 0u);
        EXPECT_EQ(nodev.hostbus.get(), 255u);
        EXPECT_EQ(nodev.hostaddr.get(), 255u);
        EXPECT_EQ(nodev_props.hostbus.get(), 254u);
        EXPECT_EQ(nodev_props.hostaddr.get(), 200u);

        bool nolibusb = logs.count("missing libusb") > 0;
        if (nolibusb) {
            log_info("hostdev built without libusb");
            EXPECT_EQ(logs.count("missing libusb"), 2u);
            EXPECT_EQ(logs.count("no USB device"), 0u);
        } else {
            log_info("hostdev built with libusb");
            EXPECT_EQ(logs.count("no USB device on bus 255 at address 255"),
                      1u);
            EXPECT_EQ(logs.count("no USB device on bus 254 at address 200"),
                      1u);
        }
    }

    void test_detached() {
        for (usb::hostdev* dev : { &unconfigured, &nodev, &nodev_props }) {
            EXPECT_FALSE(dev->usb_in.is_attached()) << dev->name();
            EXPECT_EQ(dev->usb_in.connection_speed(), USB_SPEED_NONE);
            EXPECT_FALSE(dev->is_addressed()) << dev->name();
            EXPECT_FALSE(dev->is_configured()) << dev->name();
            EXPECT_EQ(dev->get_address(), 0u) << dev->name();
        }

        EXPECT_EQ(stim.attached, 0u);
        EXPECT_EQ(stim.detached, 0u);
        EXPECT_FALSE(stim.usb_out.is_attached());
    }

    void test_commands() {
        for (usb::hostdev* dev : { &unconfigured, &nodev }) {
            string s = execute(*dev, "usb_attach", {}, false);
            EXPECT_EQ(s, "cannot get usb speed from device descriptor\n");
            execute(*dev, "usb_attach", { "usb_in" }, false);
            execute(*dev, "usb_attach", { "nothing" }, false);
            EXPECT_FALSE(dev->usb_in.is_attached());

            s = execute(*dev, "usb_detach", {}, true);
            EXPECT_EQ(s, mkstr("detaching %s\n", dev->usb_in.name()));
            s = execute(*dev, "usb_detach", { "usb_in" }, true);
            EXPECT_EQ(s, mkstr("detaching %s\n", dev->usb_in.name()));
            s = execute(*dev, "usb_detach", { "nothing" }, false);
            EXPECT_EQ(s, "usb socket not found: nothing");
            EXPECT_FALSE(dev->usb_in.is_attached());
        }

        EXPECT_EQ(stim.attached, 0u);
        EXPECT_EQ(stim.detached, 0u);
    }

    void test_packets() {
        vector<u8> data;
        EXPECT_EQ(stim.setup(USB_REQ_IN | USB_REQ_GET_DESCRIPTOR,
                             USB_DT_DEVICE << 8, 0, 18),
                  USB_RESULT_NACK);
        EXPECT_EQ(stim.in(0, data, 8), USB_RESULT_NACK);
        EXPECT_EQ(stim.out(0, data), USB_RESULT_NACK);
        EXPECT_EQ(stim.in(1, data, 64), USB_RESULT_NACK);
        EXPECT_EQ(stim.out(2, data), USB_RESULT_NACK);
        EXPECT_EQ(stim.set_address(1), USB_RESULT_NACK);
        EXPECT_EQ(stim.addr, 0u);

        for (usb::hostdev* dev : { &unconfigured, &nodev, &nodev_props }) {
            u16 rq = USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_DESCRIPTOR;
            EXPECT_EQ(setup(*dev, rq, USB_DT_DEVICE << 8, 0, 18),
                      USB_RESULT_NODEV);
            EXPECT_EQ(in(*dev, 0, 8), USB_RESULT_STALL);

            rq = USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_STATUS;
            EXPECT_EQ(setup(*dev, rq, 0, 0, 2), USB_RESULT_NODEV);

            rq = USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_ADDRESS;
            EXPECT_EQ(setup(*dev, rq, 5, 0, 0), USB_RESULT_SUCCESS);
            EXPECT_EQ(in(*dev, 0, 0), USB_RESULT_NODEV);
            EXPECT_FALSE(dev->is_addressed());

            rq = USB_REQ_OUT | USB_REQ_DEVICE | USB_REQ_SET_CONFIGURATION;
            EXPECT_EQ(setup(*dev, rq, 1, 0, 0), USB_RESULT_SUCCESS);
            EXPECT_EQ(in(*dev, 0, 0), USB_RESULT_NODEV);
            EXPECT_FALSE(dev->is_configured());

            rq = USB_REQ_OUT | USB_REQ_VENDOR | USB_REQ_DEVICE | 0x42;
            EXPECT_EQ(setup(*dev, rq, 0, 0, 4), USB_RESULT_SUCCESS);
            EXPECT_EQ(out(*dev, 0, 4), USB_RESULT_SUCCESS);
            EXPECT_EQ(in(*dev, 0, 0), USB_RESULT_NODEV);

            EXPECT_EQ(setup(*dev, 0, 0, 0, 0), USB_RESULT_SUCCESS);
            EXPECT_EQ(in(*dev, 0, 0), USB_RESULT_NODEV);

            EXPECT_EQ(in(*dev, 1, 64), USB_RESULT_NODEV);
            EXPECT_EQ(out(*dev, 1, 64), USB_RESULT_NODEV);
            EXPECT_EQ(in(*dev, 15, 512), USB_RESULT_NODEV);
            EXPECT_EQ(out(*dev, 2, 0), USB_RESULT_NODEV);
            EXPECT_FALSE(dev->usb_in.is_attached());
        }
    }

    void test_reset() {
        stim.usb_out.reset_device();
        stim.usb_out.reset_endpoint(0);
        stim.usb_out.reset_endpoint(1);
        EXPECT_FALSE(unconfigured.usb_in.is_attached());

        for (usb::hostdev* dev : { &unconfigured, &nodev, &nodev_props }) {
            usb_dev_if& dif = *dev;
            dif.usb_reset_device();
            dif.usb_reset_endpoint(0);
            dif.usb_reset_endpoint(2);
            EXPECT_FALSE(dev->is_addressed());
            EXPECT_FALSE(dev->usb_in.is_attached());

            u16 rq = USB_REQ_IN | USB_REQ_DEVICE | USB_REQ_GET_DESCRIPTOR;
            EXPECT_EQ(setup(*dev, rq, USB_DT_DEVICE << 8, 0, 18),
                      USB_RESULT_NODEV);
            dif.usb_reset_device();
        }

        EXPECT_EQ(stim.attached, 0u);
        EXPECT_EQ(stim.detached, 0u);
    }
};

TEST(usb_hostdev, simulate) {
    broker broker("test");
    broker.define("harness.nodev_props.hostbus", "254");
    broker.define("harness.nodev_props.hostaddr", "200");

    hostdev_test test("harness");
    sc_core::sc_start();
}
