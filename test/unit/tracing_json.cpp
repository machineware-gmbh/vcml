/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(tracing_json, tlm) {
    u8 data[4] = { 0x11, 0x22, 0x33, 0x44 };
    tlm_generic_payload tx;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x1000, data, sizeof(data));
    tx.set_response_status(TLM_OK_RESPONSE);
    EXPECT_EQ(
        trace_payload_to_json(tx),
        R"({"address":4096,"data":[17,34,51,68],"command":"WRITE","byte_enable":[],"streaming_width":4,"dmi_allowed":false,"response_status":"TLM_OK_RESPONSE"})");

    tlm_generic_payload tx2;
    tx_setup(tx2, TLM_READ_COMMAND, 0x20, data, 2);
    tx_set_sbi(tx2, SBI_DEBUG | sbi_cpuid(3) | sbi_privilege(1));
    EXPECT_EQ(
        trace_payload_to_json(tx2),
        R"({"address":32,"data":[17,34],"command":"READ","byte_enable":[],"streaming_width":2,"dmi_allowed":false,"response_status":"TLM_INCOMPLETE_RESPONSE","sbi":{"is_debug":true,"is_nodmi":false,"is_sync":false,"is_insn":false,"is_excl":false,"is_lock":false,"is_secure":false,"atype":"untranslated","cpuid":3,"privilege":1,"asid":18446744073709551615}})");
}

TEST(tracing_json, gpio) {
    gpio_payload tx;
    tx.state = true;
    tx.vector = GPIO_NO_VECTOR;
    EXPECT_EQ(trace_payload_to_json(tx), R"({"state":true})");
    tx.state = false;
    tx.vector = 7;
    EXPECT_EQ(trace_payload_to_json(tx), R"({"state":false,"vector":7})");
}

TEST(tracing_json, clk) {
    clk_desc tx;
    tx.period = sc_time(10, SC_NS);
    tx.polarity = true;
    tx.duty_cycle = 0.5;
    EXPECT_EQ(trace_payload_to_json(tx),
              R"({"period":10,"polarity":"posedge","duty_cycle":0.5})");
}

TEST(tracing_json, pci) {
    pci_payload tx{};
    tx.command = PCI_READ;
    tx.response = PCI_RESP_SUCCESS;
    tx.space = PCI_AS_MMIO;
    tx.addr = 0x40;
    tx.data = 0xaabbccdd;
    tx.size = 4;
    tx.debug = false;
    EXPECT_EQ(
        trace_payload_to_json(tx),
        R"({"command":"PCI_READ","response":"PCI_RESP_SUCCESS","address_space":"PCI_AS_MMIO","address":64,"data":[221,204,187,170],"debug":false})");
}

TEST(tracing_json, i2c) {
    i2c_payload tx{ I2C_DATA, I2C_ACK, 0x5a };
    EXPECT_EQ(trace_payload_to_json(tx),
              R"({"command":"I2C_DATA","response":"I2C_ACK","data":90})");
}

TEST(tracing_json, lin) {
    lin_payload tx{};
    tx.linid = 5; // two data bytes
    tx.data[0] = 1;
    tx.data[1] = 2;
    tx.status = LIN_SUCCESS;
    EXPECT_EQ(trace_payload_to_json(tx),
              R"({"linid":5,"data":[1,2],"status":"LIN_SUCCESS"})");
}

TEST(tracing_json, spi) {
    spi_payload tx{};
    tx.mosi = 0xab;
    tx.miso = 0xcd;
    tx.mask = 0xff;
    EXPECT_EQ(trace_payload_to_json(tx), R"({"miso":205,"mosi":171})");
}

TEST(tracing_json, sd) {
    sd_command cmd{};
    cmd.opcode = 17;
    cmd.argument = 0x200;
    cmd.crc = 0x2a;
    cmd.response[0] = 0x11;
    cmd.response[1] = 0x00;
    cmd.resp_len = 2;
    cmd.appcmd = false;
    cmd.spi = false;
    cmd.status = SD_OK_TX_RDY;
    EXPECT_EQ(
        trace_payload_to_json(cmd),
        R"({"command":"SD_CMD","opcode":"READ_SINGLE_BLOCK","argument":512,"crc":42,"spi":false,"response":[17,0],"status":"SD_OK_TX_RDY"})");

    sd_data rd{};
    rd.mode = SD_READ;
    rd.data = 0x42;
    rd.status.read = SDTX_OK;
    EXPECT_EQ(trace_payload_to_json(rd),
              R"({"command":"SD_DATA_READ","data":66,"status":"SDTX_OK"})");

    sd_data wr{};
    wr.mode = SD_WRITE;
    wr.data = 0x24;
    wr.status.write = SDRX_OK_COMPLETE;
    EXPECT_EQ(
        trace_payload_to_json(wr),
        R"({"command":"SD_DATA_WRITE","data":36,"status":"SDRX_OK_COMPLETE"})");
}

TEST(tracing_json, virtio) {
    vq_message tx{};
    tx.index = 3;
    tx.status = VIRTIO_OK;
    tx.in.push_back({ 0x1000, 64 });
    tx.out.push_back({ 0x2000, 16 });
    tx.out.push_back({ 0x3000, 32 });
    EXPECT_EQ(
        trace_payload_to_json(tx),
        R"({"index":3,"input_buffers":[{"addr":4096,"size":64}],"output_buffers":[{"addr":8192,"size":16},{"addr":12288,"size":32}],"status":"VIRTIO_OK"})");
}

TEST(tracing_json, serial) {
    serial_payload tx{};
    tx.data = 0x41;
    tx.mask = 0xff;
    tx.baud = SERIAL_115200BD;
    tx.width = SERIAL_8_BITS;
    tx.parity = SERIAL_PARITY_NONE;
    tx.stop = SERIAL_STOP_1;
    EXPECT_EQ(trace_payload_to_json(tx),
              R"({"data":65,"bits":8,"baud":115200,"parity":"n","stop":"1"})");
}

TEST(tracing_json, signal) {
    signal_payload<u64> u(42);
    EXPECT_EQ(trace_payload_to_json(u), R"({"data":42})");
    signal_payload<bool> b(true);
    EXPECT_EQ(trace_payload_to_json(b), R"({"data":true})");
    signal_payload<u8> c(65);
    EXPECT_EQ(trace_payload_to_json(c), R"({"data":65})");
    signal_payload<double> d(1.5);
    EXPECT_EQ(trace_payload_to_json(d), R"({"data":1.5})");
    signal_payload<string> s("say \"hi\"\n");
    EXPECT_EQ(trace_payload_to_json(s),
              "{\"data\":\"say \\\"hi\\\"\\u000a\"}");
    signal_payload<sc_dt::sc_biguint<128>> big(
        "0x123456789abcdef0123456789abcdef");
    EXPECT_EQ(trace_payload_to_json(big),
              R"({"data":"1512366075204170929049582354406559215"})");
}

TEST(tracing_json, eth) {
    eth_frame invalid;
    EXPECT_EQ(trace_payload_to_json(invalid), R"({"type":"ETHERNET_EMPTY"})");

    mac_addr dst("aa:bb:cc:dd:ee:ff");
    mac_addr src("11:22:33:44:55:66");
    eth_frame tx(dst, src, eth_frame::ETHER_TYPE_IPV4, { 1, 2, 3 });
    EXPECT_EQ(
        trace_payload_to_json(tx),
        R"({"type":"ETHERNET_IPv4","sourceaddr":"11:22:33:44:55:66","destaddr":"aa:bb:cc:dd:ee:ff","data":[170,187,204,221,238,255,17,34,51,68,85,102,8,0,1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]})");
}

TEST(tracing_json, can) {
    can_frame cc{};
    cc.canid = 0x123;
    cc.dlc = 2;
    cc.data = { 0xde, 0xad };
    EXPECT_EQ(
        trace_payload_to_json(cc),
        R"({"type":"CAN","id":291,"xlf":false,"fdf":false,"eff":false,"rtr":false,"err":false,"data":[222,173]})");

    can_frame fd{};
    fd.canid = 0x1abcdef;
    fd.eff = 1;
    fd.fdf = 1;
    fd.brs = 1;
    fd.data = { 1, 2, 3 };
    EXPECT_EQ(
        trace_payload_to_json(fd),
        R"({"type":"CAN_FD","id":28036591,"xlf":false,"fdf":true,"eff":true,"brs":true,"esi":false,"data":[1,2,3]})");

    can_frame xl{};
    xl.canid = 0x7;
    xl.xlf = 1;
    xl.sec = 1;
    xl.vcid = 9;
    xl.sdt = 2;
    xl.af = 0xcafe;
    xl.data = { 4 };
    EXPECT_EQ(
        trace_payload_to_json(xl),
        R"({"type":"CAN_XL","id":7,"xlf":true,"fdf":false,"eff":false,"sec":true,"rrs":false,"vcid":9,"sdt":2,"af":51966,"data":[4]})");
}

TEST(tracing_json, usb) {
    u8 data[3] = { 9, 8, 7 };
    usb_packet tx{};
    tx.addr = 2;
    tx.epno = 1;
    tx.token = USB_TOKEN_IN;
    tx.result = USB_RESULT_SUCCESS;
    tx.data = data;
    tx.length = sizeof(data);
    EXPECT_EQ(
        trace_payload_to_json(tx),
        R"({"token":"USB_TOKEN_IN","addr":2,"endpoint":1,"data":[9,8,7],"status":"USB_RESULT_SUCCESS"})");
}

struct json_port : sc_object {
    json_port(): sc_object("port") {}
};

TEST(tracing_json, activity) {
    json_port port;
    gpio_payload gpio{ GPIO_NO_VECTOR, true };
    trace_activity_proto<gpio_payload> act(TRACE_FW, port, gpio,
                                           sc_time(10, SC_NS));
    EXPECT_EQ(act.to_json(), R"({"state":true})");
    EXPECT_EQ(
        act.to_json_record(),
        R"({"dir":"fw","protocol":"GPIO","error":false,"tx":{"state":true}})");
}
