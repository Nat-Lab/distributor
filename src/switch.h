#ifndef DIST_SWITCH_H
#define DIST_SWITCH_H
#include "types.h"
#include "fdb.h"
#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <memory>

namespace distributor {

class Switch {
protected:
    // Plug a port into a network.
    void Plug (net_t network, port_t port);

    // Unplug a port from the network. Return true if removed, false otherwise.
    bool Unplug (port_t port);

    // Check if a port is plugged into switch.
    bool Plugged (port_t port) const;

    // Foward an ethernet frame. Return false if port not plugged in. Note that
    // return true does not necessarily mean that the frame has benn forwarded.
    bool Forward (port_t src_port, const uint8_t *frame, size_t size);

    // Flush FDB entries for a port.
    void FlushFdb (port_t port);

    // Reset switch. Remove all ports, nets, FDBs.
    void Reset();

    // Send an ethernet frame to port. Need to be implement by distributor. 
    virtual void Send (port_t dst, const uint8_t *frame, size_t size) = 0;

    typedef std::unordered_map<port_t, net_t> portsmap_t;
    typedef std::unordered_multimap<net_t, port_t> netsmap_t;
    typedef std::unordered_map<net_t, std::shared_ptr<Fdb>> fdbsmap_t;
    typedef std::pair<netsmap_t::const_iterator, netsmap_t::const_iterator> ports_iter_t;

private:
    // don't be confused by portsmap_t and netsmap_t. The key type in portsmap_t
    // is port_t, but value type is net_t, thus GetNetsByPort return portsmap_t.
    portsmap_t::const_iterator GetNetByPort (port_t port) const;
    ports_iter_t GetPortsByNet (net_t net) const;

    // Get FDB by net. If FDB does not exist for that net, a new one will be
    // created.
    fdbsmap_t::iterator GetFdbByNet (net_t net);

    // Flush FDB, private version. No write mutex.
    void FlushFdbPriv (net_t net, port_t port);

    // Relay an ethernet frame to every ports on a network.
    void Broadcast (port_t src_port, net_t net, const uint8_t *frame, size_t size);

    // Check if an ethernet address is broadcast.
    static bool IsBroadcast (const struct ether_addr &addr);

    // port to network mapping
    portsmap_t _ports;

    // network to ports mapping (for broadcasting)
    netsmap_t _nets;

    // network to fdb mapping
    fdbsmap_t _fdbs;
};

}

#endif // DIST_SWITCH_H