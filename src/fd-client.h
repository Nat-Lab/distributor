#ifndef DIST_FD_CLIENT_H
#define DIST_FD_CLIENT_H
#include "distributor-client.h"

namespace distributor {

// FdClient: A Distributor Client acts like a TAP FD.
class FdClient : public DistributorClient {
public:
    FdClient (in_addr_t server_addr, in_port_t port, net_t net);

    // Get FD. -1 will be returned if not avaliable. (Do Start())
    int GetFd () const;

private:
    bool NicStart ();
    bool NicStop ();
    ssize_t NicRead (uint8_t *buffer, size_t buf_sz);
    ssize_t NicWrite (const uint8_t *buffer, size_t sz);

    int _fds[2];
    bool _started = false;
};

}

#endif // DIST_FD_CLIENT_H