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

#include "vcml/debugging/loader.h"
#include "vcml/module.h"

namespace vcml { namespace debugging {

    const char* image_type_to_str(image_type type) {
        switch (type) {
        case IMAGE_ELF: return "elf";
        case IMAGE_BIN: return "bin";
        default:
            return "unknown";
        }
    }

    image_type detect_image_type(const string& filename) {
        ifstream file(filename, std::ios::in | std::ios::binary);
        if (!file)
            return IMAGE_BIN;

        u32 head = 0;
        file.read((char*)&head, sizeof(head));
        if (!file)
            return IMAGE_BIN;

        switch (head) {
        case fourcc("\x7f""ELF"):
            return IMAGE_ELF;
        default:
            return IMAGE_BIN;
        }
    }

    vector<image_info> images_from_string(const string& s) {
        vector<image_info> images;
        vector<string> token = split(s);
        for (string cur : token) {
            cur = trim(cur);
            if (cur.empty())
                continue;

            vector<string> vec = split(cur, '@');
            if (vec.empty())
                continue;

            string file = trim(vec[0]);
            image_type type = IMAGE_BIN;
            u64 offset = 0;

            if (file_exists(file))
                type = detect_image_type(file);

            if (vec.size() > 1)
                offset = strtoull(trim(vec[1]).c_str(), NULL, 0);

            images.push_back({file, type, offset});
        }

        for (auto& ii : images)
            printf("%s @ 0x%lx\n", ii.filename.c_str(), ii.offset);

        return images;
    }

    bool loader::cmd_load(const vector<string>& args, ostream& os) {
        string image = args[0];
        u64 offset = 0ull;

        if (args.size() > 1)
            offset = strtoull(args[1].c_str(), NULL, 0);

        load_image(image, offset);
        return true;
    }

    bool loader::cmd_load_bin(const vector<string>& args, ostream& os) {
        string image = args[0];
        u64 offset = 0ull;

        if (args.size() > 1)
            offset = strtoull(args[1].c_str(), NULL, 0);

        load_image(image, offset, IMAGE_BIN);
        return true;
    }

    bool loader::cmd_load_elf(const vector<string>& args, ostream& os) {
        string image = args[0];
        u64 offset = 0ull;

        if (args.size() > 1)
            offset = strtoull(args[1].c_str(), NULL, 0);

        load_image(image, offset, IMAGE_ELF);
        return true;
    }

    void loader::load_bin(const string& filename, u64 offset) {
        ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
        VCML_REPORT_ON(!file, "cannot open file");

        u64 size = file.tellg();
        file.seekg(0, std::ios::beg);

        log_debug("loading binary file '%s' (%lu bytes) to offset 0x%lx",
                  filename.c_str(), size, offset);

        // let model allocate our image copy buffer first; this way we can load
        // directly into DMI memory, if the model can get a pointer for us
        u8* image = allocate_image(size, offset);
        if (image) {
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
        elf_reader reader(filename);
        log_debug("loading elf file '%s' with %zu segments to offset 0x%016lx",
                  filename.c_str(), reader.segments().size(), offset);

        for (auto seg : reader.segments()) {
            log_debug("loading elf segment 0x%016lx..0x%016lx", seg.phys,
                      seg.phys + seg.size - 1);

            u8* image = allocate_image(seg.size, seg.phys + offset);
            if (image) {
                reader.read_segment(seg, image);
            } else {
                vector<u8> buffer(seg.size);
                reader.read_segment(seg, buffer.data());
                copy_image(buffer.data(), seg.size, seg.phys + offset);
            }
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

    loader::loader(const string& name):
        m_name(name) {
        if (stl_contains(s_loaders, name))
            VCML_ERROR("image loader '%s' already exists", name.c_str());

        s_loaders[name] = this;

        module* m = dynamic_cast<module*>(find_object(name));
        if (m != nullptr) {
            m->register_command("load", 1, this, &loader::cmd_load,
                "load <image> [offset] to load the contents of file <image> "
                "to memory with an optional offset");
            m->register_command("load_bin", 1, this, &loader::cmd_load_bin,
                "load_bin <image> [offset] to load the binary file <image> "
                "to memory with an optional offset");
            m->register_command("load_elf", 1, this, &loader::cmd_load_elf,
                "load_elf <image> [offset] to load the ELF file <image> "
                "to memory with an optional offset");
        }
    }

    loader::~loader() {
        s_loaders.erase(m_name);
    }

    void loader::load_image(const string& file, u64 offset) {
        load_image(file, offset, detect_image_type(file));
    }

    void loader::load_image(const string& file, u64 offset, image_type type) {
        load_image(image_info{file, type, offset});
    }

    void loader::load_image(const image_info& image) {
        if (!file_exists(image.filename))
            VCML_REPORT("file not found");

        switch (image.type) {
        case IMAGE_ELF: load_elf(image.filename, image.offset); break;
        case IMAGE_BIN: load_bin(image.filename, image.offset); break;
        default:
            VCML_REPORT("unknown image type %d", (int)image.type);
        }
    }

    void loader::load_images(const string& files) {
        auto images = images_from_string(files);
        load_images(images);
    }

    void loader::load_images(const vector<image_info>& images) {
        for (auto image : images) {
            try {
                load_image(image);
            } catch (std::exception& ex) {
                log_warn("failed to load image '%s': %s",
                         image.filename.c_str(), ex.what());
            } catch (...) {
                log_warn("unkown error while loading image '%s'",
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
        for (auto it : s_loaders)
            all.push_back(it.second);
        return all;
    }

}}
