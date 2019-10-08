#ifndef DIST_FDB_H
#define DIST_FDB_H
#include "types.h"
#include <stdint.h>
#include <net/ethernet.h>
#include <map>

namespace distributor {

class FdbEntry {
public:
    FdbEntry(port_t port, const ether_addr_t *addr);
    time_t GetAge () const;
    port_t GetPort () const;

    bool operator== (const FdbEntry &other) const;
    bool DestinationIs (const struct ether_addr *addr) const;

private:
    time_t _last_seen;
};

class Fdb {
public:
    // Insert a forwarding database entry for port.
    bool Insert (port_t port, const ether_addr_t *addr);

    // Remove a forwarding database entry for port.
    bool Delete (port_t port, const ether_addr_t *addr);

    // Remove all forwarding database entries for port.
    int Discard (port_t port);

private:
    // port to etheraddr mapping (fdb)
    std::multimap<port_t, FdbEntry> _fdb;

};

}

#endif // DIST_FDB_H