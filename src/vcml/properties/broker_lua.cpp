/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/properties/broker_lua.h"
#include "vcml/logging/logger.h"
#include "vcml/core/version.h"

#include <lua.hpp>

namespace vcml {

static logger& lua_logger() {
    static logger log("lua");
    return log;
}

static broker_lua* lua_broker(lua_State* lua) {
    return static_cast<broker_lua*>(lua_touserdata(lua, lua_upvalueindex(1)));
}

static int do_debug(lua_State* lua) {
    if (!lua_isstring(lua, -1)) {
        lua_pushstring(lua, "error: vcml.debug expects a string argument");
        lua_error(lua);
        return 0;
    }

    lua_logger().debug("%s", lua_tostring(lua, -1));
    return 0;
}

static int do_info(lua_State* lua) {
    if (!lua_isstring(lua, -1)) {
        lua_pushstring(lua, "error: vcml.info expects a string argument");
        lua_error(lua);
        return 0;
    }

    lua_logger().info("%s", lua_tostring(lua, -1));
    return 0;
}

static int do_warn(lua_State* lua) {
    if (!lua_isstring(lua, -1)) {
        lua_pushstring(lua, "error: vcml.warn expects a string argument");
        lua_error(lua);
        return 0;
    }

    lua_logger().warn("%s", lua_tostring(lua, -1));
    return 0;
}

static int do_error(lua_State* lua) {
    if (!lua_isstring(lua, -1)) {
        lua_pushstring(lua, "error: vcml.error expects a string argument");
        lua_error(lua);
        return 0;
    }

    lua_logger().error("%s", lua_tostring(lua, -1));
    return 0;
}

static int do_define(lua_State* lua) {
    if (!lua_isstring(lua, -1) || !lua_isstring(lua, -2)) {
        lua_pushstring(lua, "error: vcml.define expects two string arguments");
        lua_error(lua);
        return 0;
    }

    lua_broker(lua)->define(lua_tostring(lua, -2), lua_tostring(lua, -1));
    return 0;
}

static int do_lookup(lua_State* lua) {
    if (!lua_isstring(lua, -1)) {
        lua_pushstring(lua, "error: vcml.lookup expects a string argument");
        lua_error(lua);
        return 0;
    }

    string value, name = lua_tostring(lua, -1);
    if (lua_broker(lua)->lookup(name, value))
        lua_pushstring(lua, value.c_str());
    else
        lua_pushnil(lua);
    return 1;
}

static bool g_define_globals = []() -> bool {
    auto env = mwr::getenv("VCML_LUA_DEFINE_GLOBALS");
    return env && *env == "1";
}();

static int do_define_globals(lua_State* lua) {
    // need to wait until the script has completed so that all globals are
    // present in the state before we can define them all
    g_define_globals = true;
    return 0;
}

static bool symbol_filtered(const string& name) {
    static const vector<string> filters = {
        "_G", "_VERSION", "utf8", "math", "package",
    };

    for (const string& filter : filters)
        if (starts_with(name, filter))
            return true;

    return false;
}

static string global_name(lua_State* lua, const string& parent) {
    string name = lua_tostring(lua, -2);

    // workaround for 'in' being a keyword in lua but also frequently used as a
    // name for our tlm target sockets
    if (name == "__in" || name == "_in")
        name = "in";

    return strcat(parent, name);
}

static void define_globals(lua_State* lua, broker* b, const string& parent) {
    MWR_ERROR_ON(!lua_istable(lua, -1), "parsing error: not a table");
    lua_pushnil(lua);
    while (lua_next(lua, -2) != 0) {
        string name = global_name(lua, parent);
        if (!symbol_filtered(name)) {
            if (lua_isstring(lua, -1))
                b->define(name, lua_tostring(lua, -1));
            else if (lua_istable(lua, -1))
                define_globals(lua, b, name + ".");
        }

        lua_pop(lua, 1);
    }
}

static void define_globals(lua_State* lua, broker* b) {
    lua_pushglobaltable(lua);
    define_globals(lua, b, "");
    lua_pop(lua, 1);
}

broker_lua::broker_lua(const string& file): broker("lua") {
    lua_State* lua = luaL_newstate();
    luaL_openlibs(lua);

    int err = luaL_loadfile(lua, file.c_str());
    VCML_REPORT_ON(err, "%s", lua_tostring(lua, -1));

    lua_newtable(lua);

    const vector<pair<string, long long>> integers = {
        { "vcml_version", VCML_VERSION },
        { "systemc_version", SYSTEMC_VERSION },
        { "pid", mwr::getpid() },
    };

    const vector<pair<string, string>> strings = {
        { "vcml_version_string", VCML_VERSION_STRING },
        { "systemc_version_string", SC_VERSION },
        { "username", mwr::username() },
        { "config", mwr::escape(mwr::filename_noext(file), "") },
        { "cfgdir", mwr::escape(mwr::dirname(file), "") },
        { "simdir", mwr::escape(mwr::progname(), "") },
        { "curdir", mwr::escape(mwr::curr_dir(), "") },
    };

    const vector<pair<string, int (*)(lua_State*)>> funcs = {
        { "debug", do_debug },   { "info", do_info },
        { "warn", do_warn },     { "error", do_error },
        { "define", do_define }, { "define_globals", do_define_globals },
        { "lookup", do_lookup },
    };

    const vector<pair<string, int (*)(lua_State*)>> methods = {
        { "__newindex", do_define },
        { "__index", do_lookup },
    };

    for (auto& [field, value] : integers) {
        lua_pushinteger(lua, value);
        lua_setfield(lua, -2, field.c_str());
    }

    for (auto& [field, value] : strings) {
        lua_pushstring(lua, value.c_str());
        lua_setfield(lua, -2, field.c_str());
    }

    for (auto& [field, value] : funcs) {
        lua_pushlightuserdata(lua, this);
        lua_pushcclosure(lua, value, 1);
        lua_setfield(lua, -2, field.c_str());
    }

    lua_newtable(lua);
    for (auto& [field, value] : methods) {
        lua_pushlightuserdata(lua, this);
        lua_pushcclosure(lua, value, 1);
        lua_setfield(lua, -2, field.c_str());
    }

    lua_setmetatable(lua, -2);
    lua_setglobal(lua, "vp");

    err = lua_pcall(lua, 0, 0, 0);
    VCML_REPORT_ON(err, "%s", lua_tostring(lua, -1));

    if (g_define_globals)
        define_globals(lua, this);

    lua_close(lua);
}

broker_lua::~broker_lua() {
    // nothing to do
}

} // namespace vcml
