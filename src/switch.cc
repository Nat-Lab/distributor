#include "switch.h"
#include "log.h"

namespace distributor {

void Switch::Plug (net_t net, port_t port) {
    log_debug("Plugging port %" PRIport " to network %" PRInet "...\n", port, net);
    log_logic("Obtaining write lock...\n");
    std::lock_guard<std::mutex> lck (_maps_write_mtx);
    log_logic("Obtained write lock.\n");

    // insert to port -> net mapping
    std::pair<portsmap_t::iterator, bool> rslt = _ports.insert(std::make_pair(port, net));

    // inserted as new entry
    if (rslt.second) {
        log_info("Port %" PRIport ": Associated with network %" PRInet ".\n", port, net);
        _nets.insert(std::make_pair(net, port));
        return;
    }

    // otherwise, port is in the map already.

    // record old network id
    net_t oldnet = rslt.first->second;

    log_logic("Old network: %" PRInet ", new network: %" PRInet ".\n", oldnet, net);

    // new network == old network?
    if (oldnet == net) {
        log_debug("Port %" PRIport " already associated wtih network %" PRInet ".\n", port, net);
        return;
    }

    log_logic("Network changed. Flushing FDB entries for port in old network...\n");
    FlushFdbPriv(net, port);

    // update network id
    rslt.first->second = net;

    // update ports map
    netsmap_t::const_iterator it = GetPortsByNet(oldnet);
    bool found = false;
    while (it != _nets.end()) {
        if (it->second == port) {
            found = true;
            log_logic("Removed port %" PRIport " from old network %" PRInet ".\n", port, net);
            it = _nets.erase(it);
        } else it++;
    }

    if (!found) {
        log_error("Port %" PRIport " was missing from net -> ports mapping.\n", port);
    }

    _nets.insert(std::make_pair(net, port));
    log_info("Port %" PRIport ": Re-associated to network %" PRInet " from %" PRInet ".\n", port, net, oldnet);
}

bool Switch::Unplug (port_t port) {
    log_debug("Unplugging port %" PRIport " from network %" PRInet "...\n", port, net);
    log_logic("Obtaining write lock...\n");
    std::lock_guard<std::mutex> lck (_maps_write_mtx);
    log_logic("Obtained write lock.\n");

    portsmap_t::const_iterator net = GetNetByPort(port);
    if (net == _ports.end()) {
        log_notice("Port %" PRIport " was not associated with any network.\n", port);
        return false;
    }

    net_t _net = net->second;
    log_logic("Flushing FDB entries for this port...\n");
    FlushFdbPriv(_net, port);
    _ports.erase(net);
    log_logic("Removed port %" PRIport " from port -> net mapping.\n", port);

    netsmap_t::const_iterator it = GetPortsByNet(_net);
    bool found = false;
    while (it != _nets.end()) {
        if (it->second == port) {
            found = true;
            log_logic("Removed port %" PRIport " from network %" PRInet ".\n", port, net);
            it = _nets.erase(it);
        } else it++;
    }

    if (!found) {
        log_error("Port %" PRIport " was missing from net -> ports mapping.\n", port);
    }

    log_info("Unplugged %" PRIport " from network %" PRInet ".\n", port, _net);
    return true;
}

}