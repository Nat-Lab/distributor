#include "fdb.h"
#include "vars.h"
#include "log.h"
#include <string.h>

namespace distributor {

FdbKey::FdbKey() {}

FdbKey::FdbKey (const struct ether_addr &addr) {
    memcpy(&_address, &addr, sizeof(struct ether_addr));
    _hash = 0;
    ssize_t sz_diff = sizeof(size_t) - sizeof(struct ether_addr);
    if (sz_diff > 0) {
        memcpy(((uint8_t *) &_hash) + sz_diff, &addr, sizeof(struct ether_addr));
    } else {
        memcpy(&_hash, ((uint8_t *) &addr) + sz_diff, sizeof(size_t));
    }
    log_logic("Hash(%s) = %zu\n", ether_ntoa(&addr), _hash);
}

const struct ether_addr* FdbKey::Ptr() const {
    return &_address;
}

const struct ether_addr& FdbKey::Ref() const {
    return _address;
}

size_t FdbKey::Hash() const {
    return _hash;
}

bool FdbKey::operator== (const FdbKey &other) const {
    return memcmp(&(other._address), &_address, sizeof(struct ether_addr)) == 0;
}

size_t FdbKeyHasher::operator() (const FdbKey &key) const {
    return key.Hash();
}

FdbValue::FdbValue (port_t port) : _port (port) {
    log_logic("FdbValue created for port %" PRIport "\n", port);
    _last_seen = time (NULL);
}

void FdbValue::Refresh () {
    log_logic("FdbValue refreshed for port %" PRIport "\n", _port);
    _last_seen = time (NULL);
}

time_t FdbValue::GetAge () const {
    return time (NULL) - _last_seen;
}

port_t FdbValue::GetPort () const {
    return _port;
}

void FdbValue::SetPort (port_t port) {
    _port = port;
}

Fdb::Fdb(net_t network) {
    _network = network;
}

port_t Fdb::Lookup (const struct ether_addr &addr) {
    log_debug("Fdb%" PRInet ": Looking up: %s\n", _network, ether_ntoa(&addr));
    fdb_t::const_iterator it = _fdb.find(FdbKey(addr));

    // not found?
    if (it == _fdb.end()) {
        log_debug("Fdb%" PRInet ": Not found: %s\n", _network, ether_ntoa(&addr));
        return 0;
    }

    // aged?
    if ((*it).second.GetAge() > DIST_FDB_AGEING) {
        log_notice("Fdb%" PRInet ": Aged: %s\n", _network, ether_ntoa(&addr));
        _fdb.erase(it);
        return 0;
    }

    port_t port = (*it).second.GetPort();

    log_debug("Fdb%" PRInet ": Found: %s, on port %" PRIport "\n", _network, ether_ntoa(&addr), port);
    return port;
}

bool Fdb::Insert (port_t port, const struct ether_addr &addr) {
    log_debug("Fdb%" PRInet ": Inserting: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);

    fdb_t::iterator it = _fdb.find(FdbKey(addr));

    if (it != _fdb.end()) {
        log_logic("Entry exists, skipping lock and update directly.\n");
        it->second.Refresh();
        it->second.SetPort(port);
        log_debug("Fdb%" PRInet ": Refreshed: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);
        return false;
    }

    std::pair<fdb_t::iterator, bool> rslt = _fdb.insert(std::make_pair(FdbKey(addr), FdbValue(port)));

    // already exist, update last seen
    if (!rslt.second) {
        (*(rslt.first)).second.Refresh(); 
        (*(rslt.first)).second.SetPort(port);
        log_debug("Fdb%" PRInet ": Refreshed: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);
    } else {
        log_info("Fdb%" PRInet ": Inserted: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);
    }
    
    return rslt.second;
}

bool Fdb::Delete (const struct ether_addr &addr) {
    log_debug("Fdb%" PRInet ": Deleting: %s\n", _network, ether_ntoa(&addr));
    fdb_t::const_iterator it = _fdb.find(FdbKey(addr));

    // not found
    if (it == _fdb.end()) {
        log_debug("Fdb%" PRInet ": Not found: %s\n", _network, ether_ntoa(&addr));
        return false;
    }

    log_info("Fdb%" PRInet ": Deleted: %s\n", _network, ether_ntoa(&addr));
    _fdb.erase(it);
    return true;
}

int Fdb::Discard (port_t port) {
    log_debug("Fdb%" PRInet ": Discarding port %" PRIport "...\n", _network, port);

    fdb_t::const_iterator it = _fdb.begin();

    int removed = 0;

    while (it != _fdb.end()) {
        if ((*it).second.GetPort() == port) {
            removed++;
            log_debug("Fdb%" PRInet ": Remove: %s@%" PRIport "\n", _network, ether_ntoa((*it).first.Ptr()), port);
            it = _fdb.erase(it);
        } else it++;
    }

    log_info("Fdb%" PRInet ": Discared port %" PRIport ". %d ports removed.\n", _network, port, removed);

    return removed;
}


}