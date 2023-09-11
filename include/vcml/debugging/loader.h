/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_DEBUGGING_LOADER_H
#define VCML_DEBUGGING_LOADER_H

#include "vcml/core/types.h"
#include "vcml/core/module.h"
#include "vcml/logging/logger.h"

namespace vcml {
namespace debugging {

enum image_type {
    IMAGE_ELF,
    IMAGE_BIN,
    IMAGE_SREC,
    IMAGE_UIMAGE,
};

const char* image_type_to_str(image_type type);
image_type detect_image_type(const string& filename);

struct image_info {
    string filename;
    image_type type;
    u64 offset;
};

vector<image_info> images_from_string(const string& s);

class loader
{
private:
    module& m_owner;
    logger& m_log;

    bool cmd_load(const vector<string>& args, ostream& os);
    bool cmd_load_bin(const vector<string>& args, ostream& os);
    bool cmd_load_elf(const vector<string>& args, ostream& os);
    bool cmd_load_srec(const vector<string>& args, ostream& os);
    bool cmd_load_uimage(const vector<string>& args, ostream& os);

    static unordered_map<string, loader*> s_loaders;

protected:
    virtual void load_bin(const string& filename, u64 offset);
    virtual void load_elf(const string& filename, u64 offset);
    virtual void load_srec(const string& filename, u64 offset);
    virtual void load_uimage(const string& filename, u64 offset);

    typedef mwr::elf::segment elf_segment;

    virtual u8* allocate_image(u64 size, u64 offset);
    virtual u8* allocate_image(const elf_segment& seg, u64 offset);

    virtual void copy_image(const u8* img, u64 size, u64 offset) = 0;
    virtual void copy_image(const u8* img, const elf_segment& seg, u64 off);

public:
    module& owner() { return m_owner; }
    const module& owner() const { return m_owner; }
    const char* loader_name() const { return m_owner.name(); }

    loader(module& owner, bool reg_cmds);
    virtual ~loader();

    void load_image(const string& filename, u64 offset = 0);
    void load_image(const string& filename, u64 offset, image_type type);
    void load_image(const image_info& image);

    void load_images(const vector<string>& images);
    void load_images(const vector<image_info>& images);

    static loader* find(const string& name);
    static vector<loader*> all();
};

} // namespace debugging
} // namespace vcml

#endif
