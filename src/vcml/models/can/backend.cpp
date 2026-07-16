/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/bridge.h"
#include "vcml/models/can/backend.h"
#include "vcml/models/can/backend_file.h"
#include "vcml/models/can/backend_tcp.h"

#ifdef HAVE_SOCKETCAN
#include "vcml/models/can/backend_socket.h"
#endif

namespace vcml {
namespace can {

backend::backend(bridge* br): m_parent(br), m_type("unknown"), log(br->log) {
    m_parent->attach(this);
}

backend::~backend() {
    if (m_parent != nullptr)
        m_parent->detach(this);
}

void backend::send_to_guest(can_frame frame) {
    m_parent->send_to_guest(std::move(frame));
}

static unordered_map<string, backend::create_fn>& all_backends() {
    static unordered_map<string, backend::create_fn> instance;
    return instance;
}

void backend::define(const string& type, create_fn fn) {
    auto& backends = all_backends();
    if (stl_contains(backends, type))
        VCML_ERROR("can backend '%s' already defined", type.c_str());
    backends[type] = std::move(fn);
}

VCML_DEFINE_CAN_BACKEND(file, backend_file::create)
VCML_DEFINE_CAN_BACKEND(tcp, backend_tcp::create)
#ifdef HAVE_SOCKETCAN
VCML_DEFINE_CAN_BACKEND(socket, backend_socket::create)
#endif

backend* backend::create(bridge* br, const string& desc) {
    size_t pos = desc.find(':');
    string type = desc.substr(0, pos);
    vector<string> args;
    if (pos != string::npos)
        args = split(desc.substr(pos + 1), ',');

    auto& backends = all_backends();
    auto it = backends.find(type);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown network backend '" << desc << "'" << std::endl
           << "the following can backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(br, args);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", desc.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", desc.c_str());
    }
}

enum can_status_flags : u32 {
    CAN_FLAG_EFF = bit(0),
    CAN_FLAG_RTR = bit(1),
    CAN_FLAG_ERR = bit(2),
    CAN_FLAG_BRS = bit(3),
    CAN_FLAG_ESI = bit(4),
    CAN_FLAG_FDF = bit(5),
    CAN_FLAG_SEC = bit(6),
    CAN_FLAG_RRS = bit(7),
    CAN_FLAG_XLF = bit(8),
};

#pragma pack(push, 1)
struct frame_header {
    u32 canid;
    u32 flags;
    u16 dlc;
    u8 sdt;
    u8 vcid;
    u32 af;
    u16 len;
};
#pragma pack(pop)

void serialize(const can_frame& f, const send_fn& send) {
    frame_header header{};
    header.canid = mwr::cpu_to_le32(f.id());

    u32 flags = 0ul;
    if (f.eff)
        flags |= CAN_FLAG_EFF;
    if (f.rtr)
        flags |= CAN_FLAG_RTR;
    if (f.err)
        flags |= CAN_FLAG_ERR;
    if (f.brs)
        flags |= CAN_FLAG_BRS;
    if (f.esi)
        flags |= CAN_FLAG_ESI;
    if (f.fdf)
        flags |= CAN_FLAG_FDF;
    if (f.sec)
        flags |= CAN_FLAG_SEC;
    if (f.rrs)
        flags |= CAN_FLAG_RRS;
    if (f.xlf)
        flags |= CAN_FLAG_XLF;
    header.flags = mwr::cpu_to_le32(flags);
    header.dlc = f.dlc;
    header.sdt = f.sdt;
    header.vcid = f.vcid;
    header.af = mwr::cpu_to_le32(f.af);
    header.len = mwr::cpu_to_le16(f.length());

    send(reinterpret_cast<u8*>(&header), sizeof(header));
    send(f.data.data(), f.length());
}

void deserialize(can_frame& f, const recv_fn& recv) {
    frame_header header{};

    recv(reinterpret_cast<u8*>(&header), sizeof(header));
    f.canid = mwr::le32_to_cpu(header.canid);
    u32 flags = mwr::le32_to_cpu(header.flags);
    f.eff = !!(flags & CAN_FLAG_EFF);
    f.rtr = !!(flags & CAN_FLAG_RTR);
    f.err = !!(flags & CAN_FLAG_ERR);
    f.brs = !!(flags & CAN_FLAG_BRS);
    f.esi = !!(flags & CAN_FLAG_ESI);
    f.fdf = !!(flags & CAN_FLAG_FDF);
    f.sec = !!(flags & CAN_FLAG_SEC);
    f.rrs = !!(flags & CAN_FLAG_RRS);
    f.xlf = !!(flags & CAN_FLAG_XLF);
    f.dlc = header.dlc;
    f.sdt = header.sdt;
    f.vcid = header.vcid;
    f.af = mwr::le32_to_cpu(header.af);
    f.data.resize(mwr::le16_to_cpu(header.len));

    recv(f.data.data(), f.data.size());
}

} // namespace can
} // namespace vcml
