#ifndef DIST_UDP_DIST_H
#define DIST_UDP_DIST_H
#include "switch.h"
#include <sys/socket.h>
#include <stdint.h>
#include <time.h>

namespace distributor {

class Client {
public:
    // construct a new client object
    Client(int fd, const sockaddr *addr, socklen_t len);

    // write to client
    ssize_t Write (const uint8_t *buffer, size_t sz) const;

    // read fron client (blocking)
    ssize_t Read (uint8_t *buffer, size_t sz, int timeout) const;
    time_t GetAge () const;
    bool Keepalive ();

private:
    int _fd;
    struct sockaddr_storage _addr;
    socklen_t _len;
    time_t _last_seen;
};

class UdpDistributor : private Switch {
public:
    UdpDistributor(in_addr_t local_addr, in_port_t local_port);
    void Start ();
    void Stop ();

private:
    void Register (const sockaddr *addr, socklen_t len);
    void Unregister (const Client &client);

    // port to client mapping
    std::map<port_t, Client> _clients;
    port_t _counter;
    int _fd;
};

}

#endif // DIST_UDP_DIST_H