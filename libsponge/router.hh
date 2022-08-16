#ifndef SPONGE_LIBSPONGE_ROUTER_HH
#define SPONGE_LIBSPONGE_ROUTER_HH

#include "network_interface.hh"

#include <optional>
#include <queue>

//! \brief A wrapper for NetworkInterface that makes the host-side
//! interface asynchronous: instead of returning received datagrams
//! immediately (from the `recv_frame` method), it stores them for
//! later retrieval. Otherwise, behaves identically to the underlying
//! implementation of NetworkInterface.

/* helper class added by zheyuan */
struct RouteItem
{
    uint8_t _PrefixLength;
    std::optional<Address> _NextHop;
    size_t _InterfaceNum;
    RouteItem() : _PrefixLength(0), _NextHop(std::nullopt), _InterfaceNum(0){}
    RouteItem(uint8_t PrefixLength, std::optional<Address> NextHop, size_t InterfaceNum)
    : _PrefixLength(PrefixLength), _NextHop(NextHop), _InterfaceNum(InterfaceNum){}          // is _NextHop(NextHop) right ?

    /* overload operator= for RouteItem */
    RouteItem& operator=(const RouteItem& Other)
    {
        if(this != &Other)
        {
            this->_PrefixLength = Other._PrefixLength;
            this->_NextHop = Other._NextHop;
            this->_InterfaceNum = Other._InterfaceNum;
        }
        return *this;
    }  
};

class AsyncNetworkInterface : public NetworkInterface {
    std::queue<InternetDatagram> _datagrams_out{};

  public:
    using NetworkInterface::NetworkInterface;

    //! Construct from a NetworkInterface
    AsyncNetworkInterface(NetworkInterface &&interface) : NetworkInterface(interface) {}

    //! \brief Receives and Ethernet frame and responds appropriately.

    //! - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
    //! - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! - If type is ARP reply, learn a mapping from the "target" fields.
    //!
    //! \param[in] frame the incoming Ethernet frame
    void recv_frame(const EthernetFrame &frame) {
        auto optional_dgram = NetworkInterface::recv_frame(frame);
        if (optional_dgram.has_value()) {
            _datagrams_out.push(std::move(optional_dgram.value()));
        }
    };

    //! Access queue of Internet datagrams that have been received
    std::queue<InternetDatagram> &datagrams_out() { return _datagrams_out; }
};

//! \brief A router that has multiple network interfaces and
//! performs longest-prefix-match routing between them.
class Router {
    //! The router's collection of network interfaces
    std::vector<AsyncNetworkInterface> _interfaces{};

    //! Send a single datagram from the appropriate outbound interface to the next hop,
    //! as specified by the route with the longest prefix_length that matches the
    //! datagram's destination address.
    void route_one_datagram(InternetDatagram &dgram);

    /* private members added by zheyuan */

    std::map<uint32_t, RouteItem> _RouteTable{};
    bool prefixMatch(const uint32_t TargetIPAddress, const uint32_t RoutePrefix, const uint8_t PrefixLength);

    /* private members added by zheyuan */

  public:
    //! Add an interface to the router
    //! \param[in] interface an already-constructed network interface
    //! \returns The index of the interface after it has been added to the router
    size_t add_interface(AsyncNetworkInterface &&interface) {
        _interfaces.push_back(std::move(interface));
        return _interfaces.size() - 1;
    }

    //! Access an interface by index
    AsyncNetworkInterface &interface(const size_t N) { return _interfaces.at(N); }

    //! Add a route (a forwarding rule)
    void add_route(const uint32_t route_prefix,
                   const uint8_t prefix_length,
                   const std::optional<Address> next_hop,
                   const size_t interface_num);

    //! Route packets between the interfaces
    void route();
};

#endif  // SPONGE_LIBSPONGE_ROUTER_HH
