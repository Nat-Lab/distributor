#ifndef DIST_SWITCH_H
#define DIST_SWITCH_H
#include "types.h"
#include "fdb.h"
#include <stdint.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace distributor {

class Switch {
protected:
    // Plug a port into a network.
    void Plug (net_t network, port_t port);

    // Unplug a port from the network. Return true if removed, false otherwise.
    bool Unplug (port_t port);

    // Foward an ethernet frame.
    void Forward (port_t src_port, const uint8_t *frame, size_t size);

    // Flush FDB entries for a port.
    void FlushFdb (port_t port);

    // Send an ethernet frame to port. Should be implement by distributor. 
    // Return -1 on error, return bytes written on success.
    virtual ssize_t Send (port_t dst, const uint8_t *frame, size_t size) = 0;

    typedef std::map<port_t, net_t> portsmap_t;
    typedef std::unordered_multimap<net_t, port_t> netsmap_t;
    typedef std::map<net_t, Fdb> fdbsmap_t;

private:
    // don't be confused by portsmap_t and netsmap_t. The key type in portsmap_t
    // is port_t, but value type is net_t, thus GetNetsByPort return portsmap_t.
    portsmap_t::const_iterator GetNetByPort (port_t port) const;
    netsmap_t::const_iterator GetPortsByNet (net_t net) const;

    // Flush FDB, private version. No write mutex.
    void FlushFdbPriv (net_t net, port_t port);

    // port to network mapping
    portsmap_t _ports;

    // network to ports mapping
    netsmap_t _nets;

    // network to fdb mapping
    fdbsmap_t _fdbs;

    // mutex for maps
    std::mutex _maps_write_mtx;
};

}

#endif // DIST_SWITCH_H