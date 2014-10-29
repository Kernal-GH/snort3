/*
** Copyright (C) 2002-2013 Sourcefire, Inc.
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
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



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "framework/codec.h"
#include "codecs/codec_module.h"
#include "codecs/codec_events.h"
#include "protocols/protocol_ids.h"
#include "detection/fpdetect.h"
#include "main/snort.h"


namespace
{

#define CD_NO_NEXT_NAME "ipv6_no_next"
#define CD_NO_NEXT_HELP "sentinel codec"

class Ipv6NoNextCodec : public Codec
{
public:
    Ipv6NoNextCodec() : Codec(CD_NO_NEXT_NAME){};
    ~Ipv6NoNextCodec() {};


    bool decode(const RawData&, CodecData&, DecodeData&) override;
    void get_protocol_ids(std::vector<uint16_t>&) override;
};


} // namespace


bool Ipv6NoNextCodec::decode(const RawData& raw, CodecData& codec, DecodeData&)
{
    // No need ot check IPv6 extension order since this is automatically
    // the last extension.
    if (raw.len < ip::MIN_EXT_LEN)
        return false;

    // The size of this packets data should be zero.  So, set this layer's
    // length and the packet's remaining length to the same number.
    const_cast<uint32_t&>(raw.len) = ip::MIN_EXT_LEN;
    codec.lyr_len = ip::MIN_EXT_LEN;
    codec.proto_bits |= PROTO_BIT__IP6_EXT; // check for any IP related rules
    return true;
}


void Ipv6NoNextCodec::get_protocol_ids(std::vector<uint16_t>& v)
{ v.push_back(IPPROTO_ID_NONEXT); }



//-------------------------------------------------------------------------
// api
//-------------------------------------------------------------------------

static Codec* ctor(Module*)
{ return new Ipv6NoNextCodec(); }

static void dtor(Codec *cd)
{ delete cd; }

static const CodecApi no_next_api =
{
    {
        PT_CODEC,
        CD_NO_NEXT_NAME,
        CD_NO_NEXT_HELP,
        CDAPI_PLUGIN_V0,
        0,
        nullptr,
        nullptr,
    },
    nullptr, // pinit
    nullptr, // pterm
    nullptr, // tinit
    nullptr, // tterm
    ctor, // ctor
    dtor, // dtor
};


#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &no_next_api.base,
    nullptr
};
#else
const BaseApi* cd_no_next = &no_next_api.base;
#endif