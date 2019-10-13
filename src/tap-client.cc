#include "tap-client.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace distributor {

TapClient::TapClient (const char *tap_name, size_t tap_name_sz, int tap_mtu, in_addr_t server_addr, in_port_t port, net_t net) : DistributorClient(server_addr, port, net) {
    if (tap_name_sz >= IF_NAMESIZE) {
        log_warn("TAP name '%s' too long, will be truncate.\n", tap_name);
    }
    strncpy(_tap_name, tap_name, IFNAMSIZ);
    _started = false;
    _tap_mtu = tap_mtu;
}

const char* TapClient::GetTapName() const {
    return _tap_name;
}

bool TapClient::NicStart () {
    if (_started) {
        log_info("Already started.\n");
        return false;
    }

    log_info("Allocating TAP interface...\n");

    _fd = open("/dev/net/tun", O_RDWR);
    _started = true;

    if (_fd < 0) {
        log_fatal("Failed to open /dev/net/tun (%s). The client WILL NOT work.\n", strerror(errno));
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, _tap_name, IFNAMSIZ);

    int ioctl_ret = ioctl(_fd, TUNSETIFF, &ifr);
    if (ioctl_ret < 0) {
        log_fatal("TUNSETIFF ioctl(): %s. The client WILL NOT work.\n", strerror(errno));
        return false;
    }

    strncpy(_tap_name, ifr.ifr_name, IFNAMSIZ);
    log_info("TAP opened: %s\n", _tap_name);

    int iofd = socket(AF_INET, SOCK_DGRAM, 0);
    if (iofd < 0) {
        log_warn("socket(): %s. The client MIGHT NOT work properly.\n", strerror(errno));
    }

    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl_ret = ioctl(iofd, SIOCSIFFLAGS, &ifr);
    if (ioctl_ret < 0) {
        log_warn("SIOCSIFFLAGS (IFF_UP) ioctl(): %s. The client MIGHT NOT work properly.\n", strerror(errno));
    }

    if (_tap_mtu < 0) {
        log_fatal("Invalid TAP MTU %d.\n", _tap_mtu);
        return false;
    }

    if (_tap_mtu % 1400 != 0) {
        log_notice("TAP MTU is %d. Use multiple of 1400 to get the best performance.\n", _tap_mtu);
    }

    ifr.ifr_ifru.ifru_mtu = _tap_mtu;
    ioctl_ret = ioctl(iofd, SIOCSIFMTU, (caddr_t) &ifr);
    if (ioctl_ret < 0) {
        log_warn("SIOCSIFMTU ioctl(): %s. The client MIGHT NOT work properly.\n", strerror(errno));
    }

    close(iofd);

    return true;
}

bool TapClient::NicStop () {
    if (!_started) {
        log_info("Not started.\n");
        return false;
    }
    log_info("Deallocate TAP interface...\n");
    int ioctl_ret = ioctl(_fd, TUNSETPERSIST, 0);
    if (ioctl_ret < 0) {
        log_warn("TUNSETPERSIST (0) ioctl(): %s. \n", strerror(errno));
        return false;
    }

    close(_fd);
    _started = false;
    return true;
}

ssize_t TapClient::NicRead (uint8_t *buffer, size_t buf_sz) {
    return read(_fd, buffer, buf_sz);
}

ssize_t TapClient::NicWrite (const uint8_t *buffer, size_t buf_sz) {
    return write(_fd, buffer, buf_sz);
}

}