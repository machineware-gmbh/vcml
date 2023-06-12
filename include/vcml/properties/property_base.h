/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROPERTY_BASE_H
#define VCML_PROPERTY_BASE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

class property_base : public sc_attr_base
{
private:
    sc_object* m_parent;
    string m_fullname;

public:
    property_base(const char* name);
    property_base(sc_object* parent, const char* name);
    virtual ~property_base();
    virtual const char* kind() const { return "vcml::property"; }

    property_base() = delete;
    property_base(const property_base&) = delete;
    property_base& operator=(const property_base&) = delete;

    sc_object* parent() const { return m_parent; }
    const char* basename() const { return name().c_str(); }
    const char* fullname() const { return m_fullname.c_str(); }

    virtual void reset() = 0;

    virtual const char* str() const = 0;
    virtual void str(const string& s) = 0;

    virtual size_t size() const = 0;
    virtual size_t count() const = 0;
    virtual const char* type() const = 0;
};

} // namespace vcml

#endif
