/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
// ips_luajit.cc author Russ Combs <rucombs@cisco.com>

#include <lua.hpp>

#include "main/snort_types.h"
#include "helpers/chunk.h"
#include "managers/ips_manager.h"
#include "managers/plugin_manager.h"
#include "managers/script_manager.h"
#include "hash/sfhashfcn.h"
#include "parser/parser.h"
#include "framework/cursor.h"
#include "framework/module.h"
#include "framework/parameter.h"
#include "time/profiler.h"
#include "detection/detection_defines.h"

static THREAD_LOCAL ProfileStats luaIpsPerfStats;

#define opt_eval "eval"

//-------------------------------------------------------------------------
// ffi stuff
//-------------------------------------------------------------------------

struct SnortBuffer
{
    const char* type;
    const uint8_t* data;
    unsigned len;
};

extern "C" {
// ensure Lua can link with this
const SnortBuffer* get_buffer();
}

static THREAD_LOCAL Cursor* cursor;
static THREAD_LOCAL SnortBuffer buf;

SO_PUBLIC const SnortBuffer* get_buffer()
{
    assert(cursor);
    buf.type = cursor->get_name();
    buf.data = cursor->start();
    buf.len = cursor->length();
    return &buf;
}

//-------------------------------------------------------------------------
// module stuff
//-------------------------------------------------------------------------

static const Parameter s_params[] =
{
    { "~", Parameter::PT_STRING, nullptr, nullptr,
      "luajit arguments" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

#define s_help \
    "rule option for detecting with Lua scripts"

class LuaJitModule : public Module
{
public:
    LuaJitModule(const char* name) : Module(name, s_help, s_params)
    { };

    bool begin(const char*, int, SnortConfig*) override;
    bool set(const char*, Value&, SnortConfig*) override;

    ProfileStats* get_profile() const override
    { return &luaIpsPerfStats; };

public:
    std::string args;
};

bool LuaJitModule::begin(const char*, int, SnortConfig*)
{
    args.clear();
    return true;
}

bool LuaJitModule::set(const char*, Value& v, SnortConfig*)
{
    args = v.get_string();

    // if args not empty, it has to be a quoted string
    // so remove enclosing quotes
    if ( args.size() > 1 )
    {
        args.erase(0, 1);
        args.erase(args.size()-1);
    }

    return true;
}

//-------------------------------------------------------------------------
// option stuff
//-------------------------------------------------------------------------

class LuaJitOption : public IpsOption
{
public:
    LuaJitOption(const char* name, std::string& chunk, LuaJitModule*);
    ~LuaJitOption();

    uint32_t hash() const override;
    bool operator==(const IpsOption&) const override;

    int eval(Cursor&, Packet*) override;

private:
    void init(const char*, const char*);

    std::string config;
    struct lua_State** lua;
};

LuaJitOption::LuaJitOption(
    const char* name, std::string& chunk, LuaJitModule* mod)
    : IpsOption(name)
{
    // create an args table with any rule options
    config = "args = { ";
    config += mod->args;
    config += "}";

    unsigned max = get_instance_max();

    lua = new lua_State*[max];

    for ( unsigned i = 0; i < max; ++i )
        init_chunk(lua[i], chunk, name, config);
}

LuaJitOption::~LuaJitOption()
{
    unsigned max = get_instance_max();

    for ( unsigned i = 0; i < max; ++i )
        term_chunk(lua[i]);

    delete[] lua;
}

uint32_t LuaJitOption::hash() const
{
    uint32_t a = 0, b = 0, c = 0;
    mix_str(a,b,c,get_name());
    mix_str(a,b,c,config.c_str());
    final(a,b,c);
    return c;
}

bool LuaJitOption::operator==(const IpsOption& ips) const
{
    LuaJitOption& rhs = (LuaJitOption&)ips;

    if ( strcmp(get_name(), rhs.get_name()) )
        return false;

    if ( config != rhs.config )
        return false;

    return true;
}

int LuaJitOption::eval(Cursor& c, Packet*)
{
    PROFILE_VARS;
    MODULE_PROFILE_START(luaIpsPerfStats);

    cursor = &c;

    lua_State* L = lua[get_instance_id()];
    lua_getglobal(L, opt_eval);

    if ( lua_pcall(L, 0, 1, 0) )
    {
        const char* err = lua_tostring(L, -1);
        ErrorMessage("%s\n", err);
        MODULE_PROFILE_END(luaIpsPerfStats);
        return DETECTION_OPTION_NO_MATCH;
    }
    bool result = lua_toboolean(L, -1);
    lua_pop(L, 1);

    int ret = result ? DETECTION_OPTION_MATCH : DETECTION_OPTION_NO_MATCH;
    MODULE_PROFILE_END(luaIpsPerfStats);

    return ret;
}

//-------------------------------------------------------------------------
// api stuff
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    const char* key = PluginManager::get_current_plugin();
    return new LuaJitModule(key);
}

static void mod_dtor(Module* m)
{
    delete m;
}

static IpsOption* opt_ctor(Module* m, struct OptTreeNode*)
{
    const char* key = IpsManager::get_option_keyword();
    std::string* chunk = ScriptManager::get_chunk(key);

    if ( !chunk )
        return nullptr;

    LuaJitModule* mod = (LuaJitModule*)m;
    return new LuaJitOption(key, *chunk, mod);
}

static void opt_dtor(IpsOption* p)
{
    delete p;
}

const IpsApi ips_lua_api =
{
    {
        PT_IPS_OPTION,
        "tbd",
        "Lua JIT script for IPS rule option",
        IPSAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    1, PROTO_BIT__TCP,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    opt_ctor,
    opt_dtor,
    nullptr
};

const IpsApi* ips_luajit = &ips_lua_api;
