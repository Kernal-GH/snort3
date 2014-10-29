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
// layer.h author Josh Rosenbaum <jrosenba@cisco.com>

#ifndef PROTOCOLS_LAYER_H
#define PROTOCOLS_LAYER_H

#include <cstdint>
#include "main/snort_types.h"


struct Layer {
    const uint8_t* start;
    uint16_t prot_id;
    uint16_t length;
//    uint16_t invalid_bytes;  -- Commented out since nothing uses this
                              /*
                               * Data which should not be considered part of
                               * this layer's valid data, but must be skipped
                               * before the next layer. For instance, an invalid
                               * ip option. It should not be part of length but
                               * must be skipped before the next layer.
                               *
                               * Generally calculated by
                               *    (layers_entire_length) - length;
                               *     (ip::IP4Hdr*) ip4h->hlen()- length;
                               */
};


// forward declaring relevent structs. Since we're only return a pointer,
// there is no need for the actual header files

namespace vlan
{
struct VlanTagHdr;
}

namespace arp
{
struct EtherARP;
}

namespace gre
{
struct GREHdr;
}

namespace eapol
{
struct EtherEapol;
}

namespace eth
{
struct EtherHdr;
}

namespace ip
{
class IpApi;
struct IP6Frag;
}

namespace tcp
{
struct TCPHdr;
}

namespace udp
{
struct UDPHdr;
}

namespace icmp
{
struct ICMPHdr;
}

struct Packet;

namespace layer
{

//  Set by PacketManager.  Ensure you can call layer:: without a packet pointers
void set_packet_pointer(const Packet* const);


SO_PUBLIC const uint8_t* get_inner_layer(const Packet*, uint16_t proto);
SO_PUBLIC const uint8_t* get_outer_layer(const Packet*, uint16_t proto);


SO_PUBLIC const arp::EtherARP* get_arp_layer(const Packet*);
SO_PUBLIC const vlan::VlanTagHdr* get_vlan_layer(const Packet*);
SO_PUBLIC const gre::GREHdr* get_gre_layer(const Packet*);
SO_PUBLIC const eapol::EtherEapol* get_eapol_layer(const Packet*);
SO_PUBLIC const eth::EtherHdr* get_eth_layer(const Packet*);
SO_PUBLIC const uint8_t* get_root_layer(const Packet* const);
/* return a pointer to the outermost UDP layer */
SO_PUBLIC const udp::UDPHdr* get_outer_udp_lyr(const Packet* const);
// return the inner ip layer's index in the p->layers array
SO_PUBLIC int get_inner_ip_lyr_index(const Packet* const p);

// Two versions of this because ip_defrag:: wants to call this on
// its rebuilt packet, not on the current packet.  Extra function
// header will be removed once layer is a part of the Packet struct
SO_PUBLIC const ip::IP6Frag* get_inner_ip6_frag();
SO_PUBLIC const ip::IP6Frag* get_inner_ip6_frag(const Packet* const p);



// ICMP with Embedded IP layer


// Sets the Packet's api to be the IP layer which is
// embedded inside an ICMP layer.
// RETURN:
//          true - ip layer found and api set
//          false - ip layer NOT found, api reset
SO_PUBLIC bool set_api_ip_embed_icmp(const Packet*, ip::IpApi& api);
// a helper function when the api to be set is inside the packet
SO_PUBLIC bool set_api_ip_embed_icmp(const Packet* p);

/*
 *When a protocol is embedded in ICMP, these functions
 * will return a pointer to the layer.  Use the
 * proto_bits before calling these function to determine
 * what this layer is!
 */
SO_PUBLIC const tcp::TCPHdr* get_tcp_embed_icmp(const ip::IpApi&);
SO_PUBLIC const udp::UDPHdr* get_udp_embed_icmp(const ip::IpApi&);
SO_PUBLIC const icmp::ICMPHdr* get_icmp_embed_icmp(const ip::IpApi&);
/*
 * Starting from layer 'curr_layer', continuing looking at increasingly
 * outermost layer for another IP protocol.  If an IP protocol is found,
 * set the given ip_api to that layer.
 * PARAMS:
 *          Packet* = packet struct containing data
 *          ip::Api = ip api to be set
 *          int8_t curr_layer = the current, zero based layer from which to
 *                              start searching inward. After the function returs,
 *                              This field will be set to the layer before
 *                              the Ip Api.  If no IP layer is found,
 *                              it will be set to -1.
 *
 *                               0 <= curr_layer < p->num_layers
 * RETURNS:
 *          true:  if the api is set
 *          false: if the api has NOT been set
 *
 * NOTE: curr_layer is zero based.  That means to get all of the ip
 *       layers (starting from teh innermost layer), during the first call
 *       'curr_layer == p->num_layers'.
 *
 * NOTE: This functions is extremely useful in a loop
 *          while (set_inner_ip_api(p, api, layer)) { ... }
 */
SO_PUBLIC bool set_inner_ip_api(const Packet* const, ip::IpApi&, int8_t& curr_layer);


/*
 * Identical to above function except will begin searching from the
 * outermost layer until the innermost layer
 *
 * NOTE: curr_layer is zero based.  That means to get all of the ip
 *       layers (starting from the OUTERMOST layer), during the first call
 *       'curr_layer == 0'.
 */
SO_PUBLIC bool set_outer_ip_api(const Packet* const, ip::IpApi&, int8_t& curr_layer);


} // namespace layer

#endif