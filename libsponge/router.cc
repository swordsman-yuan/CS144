#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    /* 1.insert the router item into table */
    this->_RouteTable[route_prefix] = RouteItem(prefix_length, next_hop, interface_num);    // is the ctor used correctly ?
}

bool Router::prefixMatch(const uint32_t TargetIPAddress, const uint32_t RoutePrefix, const uint8_t PrefixLength){
    if(PrefixLength == 0)
        return true;
    
    /* 1. get the bits need to be shifted */
    uint8_t ShiftBit = 32 - PrefixLength;

    /* 2. shift number and compare */
    if((TargetIPAddress >> ShiftBit) == (RoutePrefix >> ShiftBit))
        return true;
    return false;
}


//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t TargetIPAddress = dgram.header().dst;
    std::optional<RouteItem> MatchItem = std::nullopt;
    for(auto iter = this->_RouteTable.begin() ; iter != this->_RouteTable.end() ; ++iter)
    {
        if(this->prefixMatch(TargetIPAddress, iter->first, iter->second._PrefixLength))
        {
			/* longest prefix match implemented here...*/
			if( !MatchItem.has_value() ||
                (MatchItem.has_value() && MatchItem.value()._PrefixLength < iter->second._PrefixLength))
				MatchItem = iter->second;
        }
    }
        
	if(MatchItem.has_value())
	{
		uint8_t TTL = dgram.header().ttl;
		if(TTL == 0 || (TTL - 1) == 0)	        // if TTL is run out, drop the datagram directly
			return;
		
        /* update the TTL as TTL-1 */
        dgram.header().ttl = (TTL-1);

		/* send out the dgram to appropriate network interface */
        if(MatchItem.value()._NextHop.has_value())
            /* next hop is not null, forward it to next router */
            this->interface(MatchItem.value()._InterfaceNum).send_datagram(dgram, MatchItem.value()._NextHop.value());            
        else    
            /* if the next hop is null, the datagram has reached its destination */
            this->interface(MatchItem.value()._InterfaceNum).send_datagram(dgram, Address::from_ipv4_numeric(TargetIPAddress));            
	}
    else
        return;         // no matching route item found, drop the datagram directly
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
