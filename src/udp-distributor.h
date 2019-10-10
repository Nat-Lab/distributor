#ifndef DIST_UDP_DIST_H
#define DIST_UDP_DIST_H
#include "switch.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <time.h>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <map>
#include <unordered_map>

namespace distributor {

class InetSocketAddress {
public:
    InetSocketAddress ();
    InetSocketAddress (const struct sockaddr_in *addr);
    bool operator== (const InetSocketAddress &other) const;
    size_t Hash() const;

private:
    struct sockaddr_in address;
};

class InetSocketAddressHasher {
public:
    size_t operator() (const FdbKey &key) const;
};

class ClientInfo {
    struct sockaddr_in address;
    net_t net;
    time_t last_seen;
};

class UdpDistributor : private Switch {
public:
    UdpDistributor(in_addr_t local_addr, in_port_t local_port);

    // Start the server
    void Start ();

    // Stop the server
    void Stop ();

    typedef std::unordered_map<InetSocketAddress, port_t, InetSocketAddressHasher> clientsmap_t;
    typedef std::map<port_t, std::shared_ptr<ClientInfo>> infomap_t;

private:
    // Worker thread
    void Worker ();

    // Register/Update a client
    void Register (const sockaddr_in &addr, net_t net);

    // Unregister a client
    void Unregister (const sockaddr_in &addr);

    // inherited
    void Send (port_t client, const uint8_t *buffer, size_t size);

    // Send frame to a client
    ssize_t SendTo (port_t client, const uint8_t *buffer, size_t size);

    // Receive frame
    ssize_t Recv (port_t *src_client, uint8_t *buffer, size_t buf_size);

    port_t _next_port;
    int _fd;
    clientsmap_t _clients;
    bool _running;
    std::vector<std::thread> _threads;
    std::mutex _write_mtx;
};

}

#endif // DIST_UDP_DIST_H