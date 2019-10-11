#ifndef DIST_UDP_DIST_H
#define DIST_UDP_DIST_H
#include "switch.h"
#include "vars.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <time.h>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <unordered_map>

#define DIST_MAGIC 0x5EED

namespace distributor {

struct dist_header {
    uint16_t magic;
    uint8_t msg_type;
} __attribute__ ((__packed__));

typedef struct dist_header dist_header_t;

enum msg_types {
    M_ETHERNET_FRAME = 0, 
    M_ASSOCIATE_REQUEST = 1, 
    M_ASSOCIATE_RESPOND = 2,
    M_KEEPALIVE_REQUEST = 3, 
    M_KEEPALIVE_RESPOND = 4,
    M_NEED_ASSOCIATION = 5,
    M_DISCONNECT = 6
};

class InetSocketAddress {
public:
    InetSocketAddress ();
    InetSocketAddress (const struct sockaddr_in &addr);
    bool operator== (const InetSocketAddress &other) const;
    size_t Hash() const;

private:
    struct sockaddr_in address;
};

class InetSocketAddressHasher {
public:
    size_t operator() (const FdbKey &key) const;
};

class Client {
public:
    Client (struct sockaddr_in &address);
    const struct sockaddr_in& AddrRef () const;
    const struct sockaddr_in* AddrPtr () const;

    // send DISCONNECT to client
    ssize_t Disconnect ();

    // send KEEPALIVE_REQUEST to client
    ssize_t Keepalive ();

    // send KEEPALIVE_RESPOND to client
    ssize_t AckKeepalive ();

    // send NEED_ASSOCIATION to client
    ssize_t Associate ();

    // send ASSOCIATE_RESPOND to client
    ssize_t AckAssociate ();

    // update last_seen value.
    void Saw ();

    // write an ethrnet frame to client
    ssize_t Write(const uint8_t *buffer, size_t size);

private:
    struct sockaddr_in address;
    time_t last_seen;
    uint8_t send_buffer[DIST_CLIENT_SEND_BUFSZ];
};

class UdpDistributor : private Switch {
public:
    UdpDistributor(uint32_t threads, in_addr_t local_addr, in_port_t local_port);

    // Start the server
    void Start ();

    // Stop the server
    void Stop ();

    // Join worker threads
    void Join ();

    typedef std::unordered_map<InetSocketAddress, port_t, InetSocketAddressHasher> clientsmap_t;
    typedef std::unordered_map<port_t, std::shared_ptr<Client>> infomap_t;

private:
    // Worker thread
    void Worker (int id);

    // inherited
    void Send (port_t client, const uint8_t *buffer, size_t size);

    in_port_t _local_port;
    in_addr_t _local_addr;
    uint32_t _n_threads;
    port_t _next_port;
    int _fd;
    clientsmap_t _clients;
    infomap_t _infos;
    bool _running;
    std::vector<std::thread> _threads;
    std::mutex _write_mtx;
};

}

#endif // DIST_UDP_DIST_H