#ifndef DIST_TAP_CLIENT_H
#define DIST_TAP_CLIENT_H
#include "distributor-client.h"
#include "log.h"
#include <net/if.h>

namespace distributor {

class TapClient : public DistributorClient {
public:
    TapClient (const char *tap_name, size_t tap_name_sz, in_addr_t server_addr, in_port_t port, net_t net, bool compression);

    // Note that TAP name does not necessarily to be tap_name specified in constructor.
    const char* GetTapName() const;

private:
    bool NicStart ();
    bool NicStop ();
    ssize_t NicRead (uint8_t *buffer, size_t buf_sz);
    ssize_t NicWrite (const uint8_t *buffer, size_t sz);

    char _tap_name[IF_NAMESIZE];
    int _fd;
    bool _started;
};

}
#endif // DIST_TAP_CLIENT_H