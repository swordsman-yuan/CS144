#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    /* 1.if the EthernetAddress is already known */
    if(this->_Cache.find(next_hop_ip) != this->_Cache.end())
    {
        /* 1.1 pack the EthernetFrame */
        EthernetFrame Frame;

        /* pack the header of EthernetFrame */
        Frame.header().type = EthernetHeader.TYPE_IPv4;
        Frame.header().src = this->_ethernet_address;
        Frame.header().dst = this->_Cache[next_hop_ip].EtherAddr;

        /* build the payload of EthernetFrame */
        Frame.payload() = dgram.serialize();

        /* 1.2 send out the frame immediately */
        this->frames_out.push(Frame);
    }

    /* 2.if the EthernetAddress is not known */
    else
    {
        /* 2.1 construct a ARP message to probe the MAC EthernetAddress */
        ARPMessage Message;

        /* set the address domain of ARP message */
        Message.sender_ip_address = this->_ip_address.ipv4_numeric();
        Message.sender_ethernet_address = this->_ethernet_address;
        Message.target_ip_address = next_hop_ip;
        Message.target_ethernet_address = ETHERNET_BROADCAST;     // dst MAC address is a BROADCAST address

        /* 2.2 pack the ARPMessage into a EthernetFrame */
        EthernetFrame Frame;
        Frame.header().type = EthernetHeader.TYPE_ARP; 
        Frame.header().src = this->_ethernet_address;
        Frame.header().dst = ETHERNET_BROADCAST;

        /* send out the ARP asking frame immediately */
        this->frames_out.push(Frame);
        this->_EthernetAddrUnknown.push()

    }
    
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }
