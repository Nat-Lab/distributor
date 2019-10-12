#include "types.h"
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#define DIST_CLIENT_BUF_SZ 65536
#define DIST_CLIENT_MAGIC 0x5EED

namespace distributor {

enum DistributorClientState {
    S_IDLE,
    S_CONNECTE,
    S_CONNECTED,
    S_ASSOCIATED,
    S_TIMEOUT
};

class DistributorClient {
public:
    DistributorClient (in_addr_t server_addr, in_port_t port, net_t net);

    // Change network
    void SetNetwork (net_t net);

    // Start client
    void Start ();

    // Disconnect and stop client
    void Stop ();

    // Join threads
    void Join ();

    // Spin up NIC
    virtual void NicStart () = 0;

    // Shutdown NIC
    virtual void NicStop () = 0;

    // Read from NIC.
    virtual ssize_t NicRead (uint8_t *buffer, size_t buf_sz) = 0;

    // Write to NIC.
    virtual ssize_t NicWrite (const uint8_t *buffer, size_t sz) = 0;

private:
    // Send a message with no payload to server.
    ssize_t SendMsg (msg_type_t msg);

    // Socket worker thread
    void SocketWorker ();

    // NIC worker thread
    void NicWorker ();
    
    // Pinger thread (send keepalive/server status checker)
    void Pinger ();

    net_t _net;
    struct sockaddr_in _server;
    std::vector<std::thread> _threads;
    int _fd;
    DistributorClientState _state;
    time_t _last_sent;
    time_t _last_recv;
    bool _running;
};

}