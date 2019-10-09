#ifndef DIST_FDB_H
#define DIST_FDB_H
#include "types.h"
#include <stdint.h>
#include <net/ethernet.h>
#include <unordered_map>
#include <mutex>

namespace distributor {

// forward-declaration for fdb key haser
struct FdbKeyHasher;

// key type for unordered_map to map ethernet address to fdb entries.
class FdbKey {
public:
    FdbKey ();
    FdbKey (const ether_addr_t &addr);
    const ether_addr_t* Ptr() const;
    const ether_addr_t& Ref() const;
    size_t Hash() const;
    bool operator== (const FdbKey &other) const;

private:
    size_t _hash;
    ether_addr_t _address;
};

// fdb value
class FdbValue {
public:
    FdbValue (port_t port);
    void Refresh ();
    time_t GetAge () const;
    port_t GetPort () const;
    void SetPort (port_t port);

private:
    port_t _port;
    time_t _last_seen;
};

// hashing function for fdb key
struct FdbKeyHasher {
    std::size_t operator() (const FdbKey &key) const;
};

// fdb
class Fdb {
public:
    typedef std::unordered_map<FdbKey, FdbValue, FdbKeyHasher> fdb_t;

    Fdb (net_t network);

    // Look up an address in fdb, return 0 if not found. (entry will be remove 
    // if aged, and 0 will be returned)
    port_t Lookup (const ether_addr_t &addr);

    // Insert a forwarding database entry for port, return true if new entry
    // created, return false if old entry updated.
    bool Insert (port_t port, const ether_addr_t &addr);

    // Remove a forwarding database entry for port.
    bool Delete (const ether_addr_t &addr);

    // Remove all forwarding database entries for port, return number of entries
    // removed.
    int Discard (port_t port);

private:
    // which network is this fdb for? (for logging only)
    net_t _network;

    // etheraddr to fdn entry mapping (fdb)
    fdb_t _fdb;

    // write mutex for fdb
    std::mutex _fdb_write_mtx;

};

}

#endif // DIST_FDB_H