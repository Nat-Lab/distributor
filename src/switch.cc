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

void Switch::Forward (port_t src_port, const uint8_t *frame, size_t size) {
    log_debug("Forwarding ethernet frame of size %zu from port %" PRIport ".\n", size, src_port);

    if (size < sizeof(ether_header_t)) {
        log_warn("Invalid ethernet frame from port %" PRIport ": size too small.\n", src_port);
        return;
    }

    log_logic("Got packet on port %" PRIport ".\n", port);
    const ether_header_t *hdr = (const ether_header_t *) frame;
    const ether_addr_t *src = (const ether_addr_t *) hdr->ether_shost;
    const ether_addr_t *dst = (const ether_addr_t *) hdr->ether_dhost;
    log_logic("SRC: %s\n", ether_ntoa(src));
    log_logic("DST: %s\n", ether_ntoa(dst));

    portsmap_t::const_iterator net_it = GetNetByPort(src_port);
    if (net_it == _ports.end()) {
        log_warn("Port %" PRIport " was not associated with any network.\n", src_port);
        return;
    }

    net_t net = net_it->second;
    fdbsmap_t::iterator fdb_it = GetFdbByNet(net);

    Fdb &fdb = fdb_it->second;

    if (!IsBroadcast(*src)) {
        log_logic("SRC address %s was not broadcast, inserting into FDB.\n", ether_ntoa(src));
        fdb.Insert(src_port, *src);
    }

    if (!IsBroadcast(*dst)) {
        log_logic("DST address %s was not broadcast, looking up from FDB.\n", ether_ntoa(dst));
        port_t dst_port = fdb.Lookup(*dst);

        if (dst_port != 0) {
            log_logic("DST address is on port %" PRIport ".\n", dst_port);

            ssize_t send_ret = Send(dst_port, frame, size);

            if (send_ret < 0) {
                log_error("Error relaying ethernet frame to port %" PRIport ": %s.\n", dst_port, strerror(errno));
                return;
            }

            if ((size_t) send_ret != size) {
                log_error("Send() on port %" PRIport " returned value (%zu) != size (%zu).\n", dst_port, (size_t) send_ret, size);
                return;
            }

            log_logic("Frame forwarded to port %" PRIport ".\n", dst_port);
            return;
        }

        log_notice("DST address %s was not in FDB, flooding all ports on network %" PRInet ".\n", ether_ntoa(dst), net);
        Broadcast(net, frame, size);
    }

    log_logic("DST address is broadcast, flooding all ports on network %" PRInet ".\n", net);
    Broadcast(net, frame, size);
}

}