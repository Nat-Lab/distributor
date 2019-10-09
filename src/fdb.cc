#include "fdb.h"
#include "vars.h"
#include "log.h"

namespace distributor {

FdbKey::FdbKey() {}

FdbKey::FdbKey (const ether_addr_t &addr) {
    memcpy(&_address, &addr, sizeof(ether_addr_t));
}

const ether_addr_t* FdbKey::Ptr() const {
    return &_address;
}

const ether_addr_t& FdbKey::Ref() const {
    return _address;
}

bool FdbKey::operator== (const FdbKey &other) const {
    return memcmp(&(other._address), &_address, sizeof(ether_addr_t)) == 0;
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
    return _last_seen - time (NULL);
}

port_t FdbValue::GetPort () const {
    return _port;
}

Fdb::Fdb(net_t network) {
    _network = network;
}

port_t Fdb::Lookup (const ether_addr_t &addr) {
    log_debug("Fdb%" PRInet ": Looking up: %s\n", _network, ether_ntoa(&addr));
    fdb_t::const_iterator it = _fdb.find(FdbKey(addr));

    // not found?
    if (it == _fdb.end()) {
        log_info("Fdb%" PRInet ": Not found: %s\n", _network, ether_ntoa(&addr));
        return 0;
    }

    // aged?
    if ((*it).second.GetAge() > DIST_FDB_AGEING) {
        log_info("Fdb%" PRInet ": Aged: %s\n", _network, ether_ntoa(&addr));
        log_logic("Fdb%" PRInet ": Obtaining write lock...\n", _network);
        std::lock_guard<std::mutex> lck (_fdb_write_mtx);
        log_logic("Fdb%" PRInet ": Obtained write lock.\n", _network);
        _fdb.erase(it);
        return 0;
    }

    port_t port = (*it).second.GetPort();

    log_debug("Fdb%" PRInet ": Found: %s, on port %" PRIport "\n", _network, ether_ntoa(&addr), port);
    return port;
}

bool Fdb::Insert (port_t port, const ether_addr_t &addr) {
    log_debug("Fdb%" PRInet ": Inserting: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);
    log_logic("Fdb%" PRInet ": Obtaining write lock...\n", _network);
    std::lock_guard<std::mutex> lck (_fdb_write_mtx);
    log_logic("Fdb%" PRInet ": Obtained write lock.\n", _network);
    std::pair<fdb_t::iterator, bool> rslt = _fdb.insert(std::make_pair(FdbKey(addr), FdbValue(port)));

    // already exist, update last seen
    if (!rslt.second) {
        (*(rslt.first)).second.Refresh(); 
        log_debug("Fdb%" PRInet ": Refreshed: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);
    } else {
        log_debug("Fdb%" PRInet ": Inserted: %s@%" PRIport "\n", _network, ether_ntoa(&addr), port);
    }
    
    return rslt.second;
}

bool Fdb::Delete (const ether_addr_t &addr) {
    log_debug("Fdb%" PRInet ": Deleting: %s\n", _network, ether_ntoa(&addr));
    fdb_t::const_iterator it = _fdb.find(FdbKey(addr));
    log_logic("Fdb%" PRInet ": Obtaining write lock...\n", _network);
    std::lock_guard<std::mutex> lck (_fdb_write_mtx);
    log_logic("Fdb%" PRInet ": Obtained write lock.\n", _network);

    // not found
    if (it == _fdb.end()) {
        log_info("Fdb%" PRInet ": Not found: %s\n", _network, ether_ntoa(&addr));
        return false;
    }

    log_debug("Fdb%" PRInet ": Deleted: %s\n", _network, ether_ntoa(&addr));
    _fdb.erase(it);
    return true;
}

int Fdb::Discard (port_t port) {
    log_debug("Fdb%" PRInet ": Discarding port %" PRIport "...\n", _network, port);
    log_logic("Fdb%" PRInet ": Obtaining write lock...\n", _network);
    std::lock_guard<std::mutex> lck (_fdb_write_mtx);
    log_logic("Fdb%" PRInet ": Obtained write lock.\n", _network);

    fdb_t::const_iterator it = _fdb.begin();

    int removed = 0;

    while (it != _fdb.end()) {
        if ((*it).second.GetPort() == port) {
            removed++;
            _fdb.erase(it);
            log_debug("Fdb%" PRInet ": Removed: %s@%" PRIport "\n", _network, ether_ntoa((*it).first.Ptr()), port);
        } else it++;
    }

    log_debug("Fdb%" PRInet ": Discared port %" PRIport ". %d ports removed.\n", _network, port, removed);

    return removed;
}


}