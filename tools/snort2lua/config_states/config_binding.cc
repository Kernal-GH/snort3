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
// config_binding.cc author Josh Rosenbaum <jrosenba@cisco.com>

#include <sstream>
#include <vector>

#include "conversion_state.h"
#include "utils/converter.h"
#include "utils/s2l_util.h"
#include "utils/util_binder.h"
#include "utils/parse_cmd_line.h"
#include "data/data_types/dt_comment.h"

namespace config
{

namespace {

class Binding : public ConversionState
{
public:
    Binding(Converter& c) : ConversionState(c) {};
    virtual ~Binding() {};
    virtual bool convert(std::istringstream& data_stream);

private:
    void add_vlan(const std::string& vlan, Binder&);
    void add_policy_id(const std::string& id, Binder&);
    void add_net(const std::string& net, Binder&);
};

} // namespace

typedef void (Binding::*binding_func)(const std::string&, Binder&);

void Binding::add_vlan(const std::string& vlan,
                        Binder& bind)
{
    const std::size_t init_pos = vlan.find_first_of('-');

    if (init_pos == std::string::npos)
    {
        std::size_t bad_char_pos;
        const int vlan_id = std::stoi(vlan, &bad_char_pos);

        if (bad_char_pos != vlan.size())
        {
            throw std::invalid_argument("Only numbers are allowed in "
                "vlan IDs!!  vlan " + vlan + "is invalid");
        }

        bind.add_when_vlan(std::to_string(vlan_id));
    }
    else
    {
        if (init_pos == 0 ||
            init_pos != vlan.find_last_of('-') ||
            (init_pos+1) == vlan.size())
        {
            throw std::invalid_argument("Vlan must be in the range [0,4095]");
        }

        std::size_t vlan1_pos;
        const int vlan1 = std::stoi(vlan.substr(0, init_pos), &vlan1_pos);

        std::size_t vlan2_pos;
        const std::string vlan2_str = vlan.substr(init_pos+1);
        const int vlan2 = std::stoi(vlan2_str, &vlan2_pos);


        if ((vlan1_pos != init_pos) ||
            (vlan2_pos != vlan2_str.size()))
        {
            throw std::invalid_argument("Only numbers are allowed in "
                "vlan IDs!!  vlan range " + vlan +  "is invalid");
        }

        if (vlan1 < 0 || vlan2 > 4095 || vlan1 > vlan2)
        {
            throw std::out_of_range("Vlan must be in the range [0,4095]");
        }


        for (int i = vlan1; i <= vlan2; ++i)
            bind.add_when_vlan(std::to_string(i));
    }
}

void Binding::add_policy_id(const std::string& id,
                            Binder& bind)
{
    std::size_t bad_char_pos;
    const int policy_id = std::stoi(id, &bad_char_pos);

    if (bad_char_pos != id.size())
    {
        throw std::invalid_argument("only numbers are allowed in Policy "
            "IDs, but policy_id is " + id);
    }

    bind.set_when_policy_id(policy_id);
}

void Binding::add_net(const std::string& net,
                        Binder& bind)
{
    bind.add_when_net(net);
}


bool Binding::convert(std::istringstream& data_stream)
{
    std::string binding_type;
    std::string file;
    std::string val;
    binding_func func;

    if ((!(data_stream >> file)) ||
        (!(data_stream >> binding_type)))
        return false;

    if (!binding_type.compare("policy_id"))
        func = &Binding::add_policy_id;

    else if (!binding_type.compare("vlan"))
        func = &Binding::add_vlan;

    else if (!binding_type.compare("net"))
        func = &Binding::add_net;

    else
        return false;

    // we need at least one argument
    if (!util::get_string(data_stream, val, ","))
        return false;

    Binder bind(table_api);
    bool rc = true;

    do
    {
        try
        {
            (this->*func)(val, bind);
        }
        catch (const std::invalid_argument& e)
        {
            data_api.failed_conversion(data_stream, val + " -- " + e.what());
            rc = false;
        }
        catch (const std::out_of_range& e)
        {
            data_api.failed_conversion(data_stream, val + " -- " + e.what());
            rc = false;
        }

    } while (util::get_string(data_stream, val, ","));



    if (cv.get_parse_includes())
    {
        std::string full_name = data_api.expand_vars(file);
        std::string full_path = full_name;

        if (!util::file_exists(full_path))
            full_path = parser::get_conf_dir() + full_name;


        if (util::file_exists(full_path))
        {
            Converter bind_cv;

            // This will ensure that the final ouput file contains
            // lua syntax - even if their are only rules in the file
            bind_cv.get_table_api().open_top_level_table("ips");
            bind_cv.get_table_api().close_table();


//            file = file + ".lua";  FIXIT-L  the file names should contain their original variables
            file = full_path + ".lua";

            if (bind_cv.convert(full_path, file, file, full_path + ".rej") < 0)
                rc = false;

        }
        else
        {
            rc = false;
        }
    }

    bind.set_use_file(file);
    return rc;
}

/**************************
 *******  A P I ***********
 **************************/

static ConversionState* ctor(Converter& c)
{ return new Binding(c); }

static const ConvertMap binding_api =
{
    "binding",
    ctor,
};

const ConvertMap* binding_map = &binding_api;

} // namespace config



#if TEST_CONFIG_BINDING



//FIXIT-H J   --   either get this working or remove from source code

#include <iostream>
void test_vlan(const std::string& str. bool result)
{
    Binding b;
    std::istringstream i(str);

    if (b->convert(i) == result)
        std::cout << "SUCCESS" << std::endl;
    else
        std::cout << "FAILED TO CONVERT -- " + str << std::endl;
}

int main()
{
    // the test string
    test_vlan("file vlan 5", true);
    test_vlan("file vlan -1", false);
    test_vlan("file vlan 10-", false);
    test_vlan("file vlan 566", true);
    test_vlan("file vlan 5777", true);
    test_vlan("file vlan 35-40", true);
    test_vlan("file vlan 55-50", false);
    test_vlan("file vlan    66abcd-   69", false);
    test_vlan("file vlan    76-   79xyzz", false);

}

#endif