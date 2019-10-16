#include "fd-client.h"
#include "log.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace distributor {

FdClient::FdClient (in_addr_t s, in_port_t p, net_t n) : DistributorClient(s, p, n) {
    _fds[0] = -1;
    _fds[1] = -1;
    _started = false;
}

int FdClient::GetFd () const {
    return _started ? _fds[1] : -1;
}

bool FdClient::NicStart () {
    int sp_ret = socketpair(AF_UNIX, SOCK_DGRAM, 0, _fds);

    if (sp_ret < 0) {
        log_fatal("Failed to create socket pair for communication.\n");
        return false;
    }

    _started = true;
    return true;
}

bool FdClient::NicStop () {
    if (_started) {
        close(_fds[0]);
        close(_fds[1]);
    } else return false;

    _started = false;
    return true;
}

ssize_t FdClient::NicRead (uint8_t *buffer, size_t buf_sz) {
    return read (_fds[0], buffer, buf_sz);
}

ssize_t FdClient::NicWrite (const uint8_t *buffer, size_t sz) {
    return write (_fds[0], buffer, sz);
}

}
