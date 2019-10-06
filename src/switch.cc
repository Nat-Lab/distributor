#include "switch.h"
#include "log.h"

namespace distributor {

void Switch::Plug (net_t net, port_t port) {
    log_debug("Plugging port %" PRIport " to network %" PRInet "...\n", port, net);

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
    ports_iter_t its = GetPortsByNet(oldnet);
    netsmap_t::const_iterator it = its.first;
    bool found = false;
    while (it != its.second) {
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
    log_debug("Unplugging port %" PRIport "...\n", port);

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

    ports_iter_t its = GetPortsByNet(_net);
    netsmap_t::const_iterator it = its.first;
    bool found = false;
    while (it != its.second) {
        if (it->second == port) {
            found = true;
            log_logic("Removed port %" PRIport " from network %" PRInet ".\n", port, _net);
            it = _nets.erase(it);
        } else it++;
    }

    if (!found) {
        log_error("Port %" PRIport " was missing from net -> ports mapping.\n", port);
    }

    log_info("Unplugged %" PRIport " from network %" PRInet ".\n", port, _net);
    return true;
}

bool Switch::Plugged (port_t port) const {
    return _ports.find(port) != _ports.end();
}

bool Switch::Forward (port_t src_port, const uint8_t *frame, size_t size) {
    log_debug("Forwarding ethernet frame of size %zu from port %" PRIport ".\n", size, src_port);

    if (size < sizeof(struct ether_header)) {
        log_warn("Invalid ethernet frame from port %" PRIport ": size too small.\n", src_port);
        return true;
    }

    log_logic("Got packet on port %" PRIport ".\n", src_port);
    const struct ether_header *hdr = (const struct ether_header *) frame;
    const struct ether_addr *src = (const struct ether_addr *) hdr->ether_shost;
    const struct ether_addr *dst = (const struct ether_addr *) hdr->ether_dhost;
    log_logic("SRC: %s\n", ether_ntoa(src));
    log_logic("DST: %s\n", ether_ntoa(dst));

    portsmap_t::const_iterator net_it = GetNetByPort(src_port);
    if (net_it == _ports.end()) {
        log_warn("Port %" PRIport " was not associated with any network.\n", src_port);
        return false;
    }

    net_t net = net_it->second;
    fdbsmap_t::iterator fdb_it = GetFdbByNet(net);

    Fdb &fdb = *(fdb_it->second);

    if (!IsBroadcast(*src)) {
        log_logic("SRC address %s was not broadcast, inserting into FDB.\n", ether_ntoa(src));
        fdb.Insert(src_port, *src);
    }

    if (!IsBroadcast(*dst)) {
        log_logic("DST address %s was not broadcast, looking up from FDB.\n", ether_ntoa(dst));
        port_t dst_port = fdb.Lookup(*dst);

        if (dst_port != 0) {
            log_logic("Forwarding frame to port %" PRIport ".\n", dst_port);
            Send(dst_port, frame, size);
            return true;
        }

        log_debug("DST address %s was not in FDB, flooding all ports on network %" PRInet ".\n", ether_ntoa(dst), net);
        Broadcast(src_port, net, frame, size);
        return true;
    }

    log_logic("DST address is broadcast, flooding all ports on network %" PRInet ".\n", net);
    Broadcast(src_port, net, frame, size);
    return true;
}

void Switch::FlushFdb(port_t port) {
    log_debug("Flusing FDB for port %" PRIport "...\n", port);
    portsmap_t::const_iterator net_it = GetNetByPort(port);
    if (net_it == _ports.end()) {
        log_warn("Port %" PRIport " was not associated with any network.\n", port);
        return;
    }

    net_t net = net_it->second;
    FlushFdbPriv(net, port);
}

void Switch::Reset () {
    log_debug("Resetting switch...\n");
    _ports.clear();
    _nets.clear();
    _fdbs.clear();
    log_debug("Switch resetted.\n");
}

Switch::portsmap_t::const_iterator Switch::GetNetByPort (port_t port) const {
    return _ports.find(port);
}

Switch::ports_iter_t Switch::GetPortsByNet (net_t net) const {
    return _nets.equal_range(net);
}

Switch::fdbsmap_t::iterator Switch::GetFdbByNet (net_t net) {
    log_debug("Getting FDB for network %" PRInet "...\n", net);
    fdbsmap_t::iterator it = _fdbs.find(net);

    if (it == _fdbs.end()) {
        log_info("FDB for network %" PRInet " does not exist, creating...\n", net);
        
        std::pair<fdbsmap_t::iterator, bool> rslt = _fdbs.insert(std::make_pair(net, std::make_shared<Fdb>(net)));

        if (!rslt.second) {
            log_error("Error inserting new FDB for network %" PRInet " .\n", net);
        }

        return rslt.first;
    }

    log_logic("FDB exists. Returning existing FDB...\n");
    return it;
}

void Switch::FlushFdbPriv (net_t net, port_t port) {
    log_debug("Flushing FDB for network %" PRInet " port %" PRIport "...\n", net, port);
    fdbsmap_t::iterator it = _fdbs.find(net);

    if (it == _fdbs.end()) {
        log_debug("No FDB exist for net %" PRInet ". Skipping flush.\n", net);
        return;
    }

    it->second->Discard(port);
}

void Switch::Broadcast (port_t src_port, net_t net, const uint8_t *frame, size_t size) {
    log_debug("Broadcast to network %" PRInet ", skipping source port %" PRIport "...\n", net, src_port);

    ports_iter_t its = GetPortsByNet(net);
    netsmap_t::const_iterator it = its.first;

    if (it == _nets.end()) {
        log_error("No ports are connected to network %" PRInet ", can't broadcast.\n", net);
        return;
    }

    for (; it != its.second; it++) {
        port_t dst_port = it->second;
        if (dst_port == src_port) continue;
        log_logic("Forwarding frame to port %" PRIport "...\n", dst_port);
        Send(dst_port, frame, size);
    }
}

bool Switch::IsBroadcast (const struct ether_addr &addr) {
    const uint16_t *a = (const uint16_t *) &addr;
    return a[0] == 0xffff && a[1] == 0xffff && a[2] == 0xffff;
}

}