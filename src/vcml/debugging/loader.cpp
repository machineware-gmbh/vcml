/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/module.h"
#include "vcml/debugging/loader.h"

namespace vcml {
namespace debugging {

const char* image_type_to_str(image_type type) {
    switch (type) {
    case IMAGE_ELF:
        return "elf";
    case IMAGE_BIN:
        return "bin";
    case IMAGE_SREC:
        return "srec";
    case IMAGE_UIMAGE:
        return "uimage";
    default:
        return "unknown";
    }
}

image_type detect_image_type(const string& filename) {
    if (ends_with(filename, ".elf") || ends_with(filename, ".so") ||
        ends_with(filename, ".out") || ends_with(filename, ".axf"))
        return IMAGE_ELF;
    if (ends_with(filename, ".srec"))
        return IMAGE_SREC;
    if (ends_with(filename, ".bin"))
        return IMAGE_BIN;

    ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file)
        return IMAGE_BIN;

    u32 head = 0;
    file.read((char*)&head, sizeof(head));
    if (!file)
        return IMAGE_BIN;

    if (head == 0x464c457f)
        return IMAGE_ELF;

    if ((head & 0xffff) == 0x3053)
        return IMAGE_SREC;

    if (head == 0x56190527)
        return IMAGE_UIMAGE;

    return IMAGE_BIN;
}

vector<image_info> images_from_string(const vector<string>& vec) {
    vector<image_info> images;
    for (string cur : vec) {
        cur = trim(cur);
        if (cur.empty())
            continue;

        size_t separator = cur.find('@');
        string file = trim(cur.substr(0, separator));
        image_type type = IMAGE_BIN;
        u64 offset = 0;

        if (mwr::file_exists(file))
            type = detect_image_type(file);

        if (separator != string::npos) {
            string extra = cur.substr(separator + 1);
            offset = strtoull(extra.c_str(), NULL, 0);
        }

        images.push_back({ file, type, offset });
    }

    return images;
}

bool loader::cmd_load(const vector<string>& args, ostream& os) {
    const string& image = args[0];
    u64 offset = 0ull;

    if (args.size() > 1)
        offset = strtoull(args[1].c_str(), NULL, 0);

    load_image(image, offset);
    return true;
}

bool loader::cmd_load_bin(const vector<string>& args, ostream& os) {
    const string& image = args[0];
    u64 offset = 0ull;

    if (args.size() > 1)
        offset = strtoull(args[1].c_str(), NULL, 0);

    load_image(image, offset, IMAGE_BIN);
    return true;
}

bool loader::cmd_load_elf(const vector<string>& args, ostream& os) {
    const string& image = args[0];
    u64 offset = 0ull;

    if (args.size() > 1)
        offset = strtoull(args[1].c_str(), NULL, 0);

    load_image(image, offset, IMAGE_ELF);
    return true;
}

bool loader::cmd_load_srec(const vector<string>& args, ostream& os) {
    const string& image = args[0];
    u64 offset = 0ull;

    if (args.size() > 1)
        offset = strtoull(args[1].c_str(), NULL, 0);

    load_image(image, offset, IMAGE_SREC);
    return true;
}

bool loader::cmd_load_uimage(const vector<string>& args, ostream& os) {
    const string& image = args[0];
    u64 offset = 0ull;

    if (args.size() > 1)
        offset = strtoull(args[1].c_str(), NULL, 0);

    load_image(image, offset, IMAGE_UIMAGE);
    return true;
}

void loader::load_bin(const string& filename, u64 offset) {
    ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    VCML_REPORT_ON(!file, "cannot open file");

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_log.debug("loading binary file '%s' (%zu bytes) to offset 0x%llx",
                filename.c_str(), size, offset);

    // let model allocate our image copy buffer first; this way we can load
    // directly into DMI memory, if the model can get a pointer for us
    if (u8* image = allocate_image(size, offset)) {
        file.read((char*)image, size);
        VCML_REPORT_ON(!file, "cannot read file");
    } else {
        const size_t bufsz = 1 * MiB;
        vector<u8> buffer(bufsz);

        while (size > 0) {
            u64 nbytes = min(buffer.size(), size);
            file.read((char*)buffer.data(), nbytes);
            VCML_REPORT_ON(!file, "cannot read file");
            copy_image(buffer.data(), nbytes, offset);
            size -= nbytes;
            offset += nbytes;
        }
    }
}

void loader::load_elf(const string& filename, u64 offset) {
    mwr::elf reader(filename);
    m_log.debug("loading elf file '%s' with %zu segments to offset 0x%016llx",
                filename.c_str(), reader.segments().size(), offset);

    for (auto seg : reader.segments()) {
        m_log.debug("loading elf segment 0x%016llx..0x%016llx", seg.phys,
                    seg.phys + seg.size - 1);

        if (u8* image = allocate_image(seg, offset)) {
            reader.read_segment(seg, image);
        } else {
            vector<u8> buffer(seg.size);
            reader.read_segment(seg, buffer.data());
            copy_image(buffer.data(), seg, offset);
        }
    }
}

void loader::load_srec(const string& filename, u64 offset) {
    mwr::srec reader(filename);

    m_log.debug("loading srec file '%s' (%zu records) to offset 0x%llx",
                filename.c_str(), reader.records().size(), offset);

    for (auto rec : reader.records()) {
        if (u8* image = allocate_image(rec.data.size(), rec.addr + offset))
            memcpy(image, rec.data.data(), rec.data.size());
        else
            copy_image(rec.data.data(), rec.data.size(), rec.addr + offset);
    }
}

void loader::load_uimage(const string& filename, u64 offset) {
    mwr::uimage reader(filename);

    m_log.debug("loading uImage file '%s' (%zu bytes) to offset 0x%llx",
                filename.c_str(), reader.size(), offset);

    if (u8* image = allocate_image(reader.size(), offset))
        reader.read(image, reader.size());
    else {
        vector<u8> buffer(reader.size());
        reader.read(buffer.data(), buffer.size());
        copy_image(buffer.data(), buffer.size(), offset);
    }
}

u8* loader::allocate_image(u64 size, u64 offset) {
    return nullptr; // to be overwritten
}

u8* loader::allocate_image(const elf_segment& seg, u64 offset) {
    return allocate_image(seg.size, seg.phys + offset);
}

void loader::copy_image(const u8* img, const elf_segment& seg, u64 off) {
    copy_image(img, seg.size, seg.phys + off);
}

unordered_map<string, loader*> loader::s_loaders;

loader::loader(module& mod, bool reg_cmds): m_owner(mod), m_log(mod.log) {
    if (stl_contains(s_loaders, string(loader_name())))
        VCML_ERROR("image loader '%s' already exists", loader_name());

    s_loaders[loader_name()] = this;

    if (!reg_cmds)
        return;

    m_owner.register_command("load", 1, this, &loader::cmd_load,
                             "load <image> [offset] to load the contents of "
                             "file <image> to memory with an optional offset");
    m_owner.register_command("load_bin", 1, this, &loader::cmd_load_bin,
                             "load_bin <image> [offset] to load the binary "
                             "file <image> to memory with an optional offset");
    m_owner.register_command("load_elf", 1, this, &loader::cmd_load_elf,
                             "load_elf <image> [offset] to load the ELF "
                             "file <image> to memory with an optional offset");
    m_owner.register_command("load_srec", 1, this, &loader::cmd_load_srec,
                             "load_srec <image> [offset] to load the SREC "
                             "file <image> to memory with an optional offset");
    m_owner.register_command("load_uimage", 1, this, &loader::cmd_load_uimage,
                             "load_uimage <image> [offset] to load the uImage "
                             "file <image> to memory with an optional offset");
}

loader::~loader() {
    s_loaders.erase(loader_name());
}

void loader::load_image(const string& file, u64 offset) {
    load_image(file, offset, detect_image_type(file));
}

void loader::load_image(const string& file, u64 offset, image_type type) {
    load_image(image_info{ file, type, offset });
}

void loader::load_image(const image_info& image) {
    if (!mwr::file_exists(image.filename))
        VCML_REPORT("file not found");

    switch (image.type) {
    case IMAGE_ELF:
        load_elf(image.filename, image.offset);
        break;
    case IMAGE_BIN:
        load_bin(image.filename, image.offset);
        break;
    case IMAGE_SREC:
        load_srec(image.filename, image.offset);
        break;
    case IMAGE_UIMAGE:
        load_uimage(image.filename, image.offset);
        break;
    default:
        VCML_REPORT("unknown image type %d", (int)image.type);
    }
}

void loader::load_images(const vector<string>& files) {
    auto images = images_from_string(files);
    load_images(images);
}

void loader::load_images(const vector<image_info>& images) {
    for (const auto& image : images) {
        try {
            load_image(image);
        } catch (std::exception& ex) {
            m_log.warn("failed to load image '%s': %s", image.filename.c_str(),
                       ex.what());
        } catch (...) {
            m_log.warn("unkown error while loading image '%s'",
                       image.filename.c_str());
        }
    }
}

loader* loader::find(const string& name) {
    auto it = s_loaders.find(name);
    return it != s_loaders.end() ? it->second : nullptr;
}

vector<loader*> loader::all() {
    vector<loader*> all;
    all.reserve(s_loaders.size());
    for (const auto& it : s_loaders)
        all.push_back(it.second);
    return all;
}

} // namespace debugging
} // namespace vcml
