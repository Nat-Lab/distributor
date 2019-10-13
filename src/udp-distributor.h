#ifndef DIST_UDP_DIST_H
#define DIST_UDP_DIST_H
#include "switch.h"
#include "vars.h"
#include "types.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <time.h>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

#define DIST_MAGIC 0x5EED

namespace distributor {

class InetSocketAddress {
public:
    InetSocketAddress ();
    InetSocketAddress (const struct sockaddr_in &addr);
    bool operator== (const InetSocketAddress &other) const;
    size_t Hash() const;
    const struct sockaddr_in& Ref() const;
    const struct sockaddr_in* Ptr() const;

private:
    struct sockaddr_in address;
    size_t hash;
};

std::size_t hash_value(const InetSocketAddress &addr);

class Client {
public:
    Client (port_t port, const InetSocketAddress &address, int fd);
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

    // check if client is alive (might sent keepalive)
    bool IsAlive ();

    // write an ethrnet frame to client
    ssize_t Write (const uint8_t *buffer, size_t size);

    const InetSocketAddress& GetAddress() const;
    port_t GetPort() const;

private:
    // send a message with no payload
    ssize_t SendMsg (msg_type_t type);

    InetSocketAddress _address;
    time_t _last_seen;
    time_t _last_sent;
    uint8_t _send_buffer[DIST_CLIENT_SEND_BUFSZ];
    int _fd;
    port_t _port;
};

struct IndexPort {};
struct IndexAddr {};

class UdpDistributor : private Switch {
public:
    UdpDistributor(in_addr_t local_addr, in_port_t local_port);

    // Start the server
    void Start ();

    // Stop the server
    void Stop ();

    // Join threads
    void Join ();

    typedef boost::multi_index_container<
        std::shared_ptr<Client>,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<IndexPort>,
                boost::multi_index::const_mem_fun<Client, port_t, &Client::GetPort>
            >,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<IndexAddr>,
                boost::multi_index::const_mem_fun<Client, const InetSocketAddress&, &Client::GetAddress>
            >
        >
    > clientdb_t;

    //typedef std::unordered_map<InetSocketAddress, port_t, InetSocketAddressHasher> clientsmap_t;
    //typedef std::unordered_map<port_t, std::shared_ptr<Client>> infomap_t;

private:
    // Worker thread
    void Worker ();

    // Scavenger thread (send keepalive to unresponsive clients and disconnect 
    // them if necessary)
    void Scavenger ();

    // inherited
    void Send (port_t client, const uint8_t *buffer, size_t size);

    in_port_t _local_port;
    in_addr_t _local_addr;
    port_t _next_port;
    int _fd;
    clientdb_t _clients;
    bool _running;
    std::vector<std::thread> _threads;
    std::mutex _scavenger_mtx;
    std::condition_variable _scavenger_cv;
};

}

#endif // DIST_UDP_DIST_H