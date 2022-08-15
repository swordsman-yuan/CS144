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

    /* 1.if the EthernetAddress is already known, send out directly */
    if(this->_Cache.find(next_hop_ip) != this->_Cache.end())
    {
        /* 1.1 pack the EthernetFrame */
        EthernetFrame Frame;

        /* pack the header of EthernetFrame */
        Frame.header().type = EthernetHeader::TYPE_IPv4;
        Frame.header().src = this->_ethernet_address;
        Frame.header().dst = this->_Cache[next_hop_ip]._EtherAddr;

        /* build the payload of EthernetFrame */
        Frame.payload() = dgram.serialize();

        /* 1.2 send out the frame immediately */
        this->frames_out().push(Frame);
    }

    /* 2.if the EthernetAddress is not known, send ARP request message to probe MAC address */
    else
    {
        if(this->_KeepARPNotFlood.find(next_hop_ip) == this->_KeepARPNotFlood.end())
        {
            /* otherwise, ready to send out ARP request */ 
            /* 2.1 construct a ARP message to probe the MAC EthernetAddress */
            ARPMessage Message;

            /* set the address domain of ARP message */
            Message.sender_ip_address = this->_ip_address.ipv4_numeric();
            Message.sender_ethernet_address = this->_ethernet_address;
            Message.target_ip_address = next_hop_ip;
            Message.target_ethernet_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};         // dst MAC address is BROADCAST address
            Message.opcode = ARPMessage::OPCODE_REQUEST;                                    // type of ARP message is REQUEST    

            /* 2.2 pack the ARPMessage into a EthernetFrame */
            /* set the frame header's field properly */
            EthernetFrame Frame;
            Frame.header().type = EthernetHeader::TYPE_ARP; 
            Frame.header().src = this->_ethernet_address;
            Frame.header().dst = ETHERNET_BROADCAST;
            Frame.payload() = BufferList(std::move(Message.serialize()));

            /* send out the ARP asking frame immediately */
            this->frames_out().push(Frame);
            this->_KeepARPNotFlood[next_hop_ip] = 0;                                            // set the timer as 0
            this->_EthernetAddrUnknown.push(EthernetAddrUnknownDatagram(next_hop_ip, dgram));   // buffer the IPDatagram waiting to be sent
        }
        /* make sure the ARP request message will not flood the network */
        else if(this->_KeepARPNotFlood[next_hop_ip] < this->_NOTFLOOD)
            return;
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    /* 1.extract the header and payload out of frame */
    EthernetHeader Header = frame.header();
    BufferList Payload = frame.payload();

    /* 2 conditions need to be handled */
    /* 2.1 if the frame's type is IPv4 */
    if(Header.type == EthernetHeader::TYPE_IPv4 && Header.dst == this->_ethernet_address)    // IP datagram
    {
        InternetDatagram IPDatagram;

        /* parse the IP datagram out */
        /* ATTENTION : I am not sure about the correctness */ 
        ParseResult Result = IPDatagram.parse(Buffer(std::move(Payload.concatenate())));      
        if(Result == ParseResult::NoError)
            return IPDatagram;
    }

    /* 2.2 if the frame's type is ARP, no matter it is request or reply */
    else if (    
                Header.type == EthernetHeader::TYPE_ARP && 
                (Header.dst == ETHERNET_BROADCAST || Header.dst == this->_ethernet_address)    
            )    // ARP message( request or reply )
    {
        ARPMessage RecvMessage;
        ParseResult Result = RecvMessage.parse(Buffer(std::move(Payload.concatenate())));  
        if(Result == ParseResult::NoError)
        {
            /* 1. extract the mapping from ARPMessage */
            uint32_t ExtractedIP = RecvMessage.sender_ip_address;
            EthernetAddress ExtractedEtherAddr = RecvMessage.sender_ethernet_address;

            /* 2. keep the mapping into cache, set the timer as 0 automatically */
            this->_Cache[ExtractedIP] = EthernetAddrRecord(ExtractedEtherAddr);

            /* 3. try to send out the IP Datagram which has been queued */
            this->sendQueuedDatagram();

             /* if a reply ARPMessage is needed... */
            if  (
                    Header.type == EthernetHeader::TYPE_ARP &&
                    Header.dst == ETHERNET_BROADCAST &&
                    RecvMessage.target_ip_address == this->_ip_address.ipv4_numeric()
                )
                {
                    ARPMessage Message;
                    /* set ARPMessage field properly */
                    Message.sender_ip_address = this->_ip_address.ipv4_numeric();
                    Message.sender_ethernet_address = this->_ethernet_address;
                    Message.target_ip_address = ExtractedIP;
                    Message.target_ethernet_address = ExtractedEtherAddr;         
                    Message.opcode = ARPMessage::OPCODE_REPLY;                                                    

                    /* 2.2 pack the ARPMessage into an EthernetFrame */
                    /* set the frame header's field properly */
                    EthernetFrame Frame;
                    Frame.header().type = EthernetHeader::TYPE_ARP; 
                    Frame.header().src = this->_ethernet_address;
                    Frame.header().dst = ExtractedEtherAddr;
                    Frame.payload() = BufferList(std::move(Message.serialize()));

                    /* send out the ARP reply frame immediately */
                    this->frames_out().push(Frame);
                }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    /* 1.increment the time record of queued datagram */
    /* and delete all the elements that has expired */
    for(auto iter = this->_KeepARPNotFlood.begin() ; iter != this->_KeepARPNotFlood.end() ; )
    {
        iter->second += ms_since_last_tick;
        if(iter->second >= this->_NOTFLOOD)
            this->_KeepARPNotFlood.erase(iter++);
        else
            ++iter;

    }

    /* 2.eliminate all the mapping which has expired */
    for(auto iter = this->_Cache.begin() ; iter != this->_Cache.end() ; )
    {
        iter->second._TimeKeeper += ms_since_last_tick;
        if(iter->second._TimeKeeper >= this->_KEEPTIME)
            this->_Cache.erase(iter++);
        else
            ++iter;
    }
}

void NetworkInterface::sendQueuedDatagram()
{
    size_t RawSize = this->_EthernetAddrUnknown.size();
    for( size_t i = 0 ; i < RawSize ; ++i )
    {
        /* get the header element from queue and pop it out */
        /* scan the cache for corresponding mapping */
        EthernetAddrUnknownDatagram Datagram = this->_EthernetAddrUnknown.front();
        this->_EthernetAddrUnknown.pop();
        if(this->_Cache.find(Datagram._IPAddress) != this->_Cache.end())
            /* try to send out the datagram if mapping is found  */
            this->send_datagram(Datagram._Datagram, Address::from_ipv4_numeric(Datagram._IPAddress));
        else
            /* no mapping is found, push the datagram back to queue */
            this->_EthernetAddrUnknown.push(Datagram);
    }
}
