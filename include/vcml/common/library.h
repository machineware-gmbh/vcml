/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_LIBRARY_H
#define VCML_LIBRARY_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/strings.h"

namespace vcml {

class library
{
private:
    string m_path;
    void* m_handle;

    void* lookup(const string& name) const;

public:
    const char* path() const { return m_path.c_str(); }
    bool is_open() const { return m_handle != nullptr; }

    library();
    library(library&& other) noexcept;
    library(const string& path, int mode);
    library(const library& copy) = delete;
    virtual ~library();

    library(const string& path): library(path, -1) {}

    void open(const string& path, int mode = -1);
    void close();

    bool has(const string& name) const;

    template <typename T>
    void get(T*& fn, const string& name) const;
};

template <typename T>
void library::get(T*& fn, const string& name) const {
    fn = (T*)lookup(name);
}

} // namespace vcml

#endif
