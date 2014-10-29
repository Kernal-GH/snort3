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
// cd_ip6_embedded_in_icmp.cc author Josh Rosenbaum <jrosenba@cisco.com>



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "framework/codec.h"
#include "protocols/protocol_ids.h"
#include "protocols/ipv6.h"
#include "protocols/packet.h"
#include "codecs/codec_module.h"
#include "codecs/codec_events.h"


// yes, macros are necessary. The API and class constructor require different strings.
//
// this macros is defined in the module to ensure identical names. However,
// if you don't want a module, define the name here.
#ifndef ICMP6_IP_NAME
#define ICMP6_IP_NAME "icmp6_ip"
#endif

#define ICMP6_IP_HELP "support for IP in ICMPv6"

namespace
{

class Icmp6IpCodec : public Codec
{
public:
    Icmp6IpCodec() : Codec(ICMP6_IP_NAME){};
    ~Icmp6IpCodec() {};


    void get_protocol_ids(std::vector<uint16_t>&) override;
    bool decode(const RawData&, CodecData&, DecodeData&) override;
};

} // namespace

void Icmp6IpCodec::get_protocol_ids(std::vector<uint16_t>& v)
{
    v.push_back(IP_EMBEDDED_IN_ICMP6);
}

bool Icmp6IpCodec::decode(const RawData& raw, CodecData& codec, DecodeData&)
{
//    uint16_t orig_frag_offset;

    /* lay the IP struct over the raw data */
    const ip::IP6Hdr* ip6h = reinterpret_cast<const ip::IP6Hdr*>(raw.data);


    /* do a little validation */
    if ( raw.len < ip::IP6_HEADER_LEN )
    {
        codec_events::decoder_event(codec, DECODE_ICMP_ORIG_IP_TRUNCATED);
        return false;
    }

    /*
     * with datalink DLT_RAW it's impossible to differ ARP datagrams from IP.
     * So we are just ignoring non IP datagrams
     */
    if(ip6h->ver() != 6)
    {
        codec_events::decoder_event(codec, DECODE_ICMP_ORIG_IP_VER_MISMATCH);
        return false;
    }

    if ( raw.len < ip::IP6_HEADER_LEN )
    {
        codec_events::decoder_event(codec, DECODE_ICMP_ORIG_DGRAM_LT_ORIG_IP);
        return false;
    }

//    orig_frag_offset = ntohs(GET_ORIG_IPH_OFF(p));
//    orig_frag_offset &= 0x1FFF;

    // XXX NOT YET IMPLEMENTED - fragments inside ICMP payload


    // since we know the protocol ID in this layer (and NOT the
    // next layer), set the correct protocol here.  Normally,
    // I would just set the next_protocol_id and let the packet_manger
    // decode the next layer. However, I  can't set the next_prot_id in
    // this case because I don't want this going to the TCP, UDP, or
    // ICMP codec. Therefore, doing a minor decode here.

    // FIXIT-J L   Will fail to decode Ipv6 options
    switch(ip6h->next())
    {
        case IPPROTO_TCP: /* decode the interesting part of the header */
            codec.proto_bits |= PROTO_BIT__TCP_EMBED_ICMP;
            break;

        case IPPROTO_UDP:
            codec.proto_bits |= PROTO_BIT__UDP_EMBED_ICMP;
            break;

        case IPPROTO_ICMP:
            codec.proto_bits |= PROTO_BIT__ICMP_EMBED_ICMP;
            break;
    }

    // if you changed lyr_len, you MUST change the encode()
    // function below to copy and update_buffer() correctly!
    codec.lyr_len = ip::IP6_HEADER_LEN;
    return true;
}


//-------------------------------------------------------------------------
// api
//-------------------------------------------------------------------------

static Codec* ctor(Module*)
{ return new Icmp6IpCodec(); }

static void dtor(Codec *cd)
{ delete cd; }


static const CodecApi icmp6_ip_api =
{
    {
        PT_CODEC,
        ICMP6_IP_NAME,
        ICMP6_IP_HELP,
        CDAPI_PLUGIN_V0,
        0,
        nullptr, // module constructor
        nullptr  // module destructor
    },
    nullptr, // g_ctor
    nullptr, // g_dtor
    nullptr, // t_ctor
    nullptr, // t_dtor
    ctor,
    dtor,
};


#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &icmp6_ip_api.base,
    nullptr
};
#else
const BaseApi* cd_icmp6_ip = &icmp6_ip_api.base;
#endif