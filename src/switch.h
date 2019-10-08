#ifndef DIST_SWITCH_H
#define DIST_SWITCH_H
#include "types.h"
#include "fdb.h"
#include <stdint.h>
#include <map>
#include <vector>

namespace distributor {

class Switch {
public:
    Switch ();

    // Plug a port into a network.
    bool Plug (net_t network, port_t port);

    // Unplug a port from a network.
    bool Unplug (net_t network, port_t port);

private:
    // port to network mapping
    std::map<port_t, net_t> _ports;

    // network to ports mapping
    std::multimap<net_t, port_t> _nets;

    // network to fdb mapping
    std::map<net_t, Fdb> _fdbs;
};

}

#endif // DIST_SWITCH_H