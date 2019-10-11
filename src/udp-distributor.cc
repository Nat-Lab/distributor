#include "udp-distributor.h"
#include "log.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace distributor {

UdpDistributor::UdpDistributor(uint32_t n_threads, in_addr_t local_addr, in_port_t local_port) {
    _local_addr = local_addr;
    _local_port = local_port;
    _n_threads = n_threads;
    _running = false;
}

void UdpDistributor::Start () {
    log_debug("Starting distributor...\n");
    if (_running) {
        log_error("Distributor was already running.\n");
        return;
    }

    struct sockaddr_in local_sockaddr;
    memset(&local_sockaddr, 0, sizeof(struct sockaddr_in));

    local_sockaddr.sin_family = AF_INET;
    local_sockaddr.sin_addr.s_addr = _local_addr;
    local_sockaddr.sin_port = _local_port;

    _fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_fd < 0) {
        log_fatal("socket(): %s\n", strerror(errno));
        return;
    }

    /*int fctl_ret = fcntl(_fd, F_SETFL, O_NONBLOCK);

    if (fctl_ret < 0) {
        log_fatal("fcntl(): %s\n", strerror(errno));
        return;
    }*/

    int bind_ret = bind(_fd, (const struct sockaddr *) &local_sockaddr, sizeof(struct sockaddr_in));

    if (bind_ret < 0) {
        log_fatal("bind(): %s\n", strerror(errno));
        return;
    }

    for (int i = 0; i < _n_threads; i++) {
        log_debug("Starting worker %d\n", i);
        _threads.push_back(std::thread(&UdpDistributor::Worker, this));
    }

    log_info("Distributor ready.\n");

    _running = true;
}

void UdpDistributor::Stop () {
    log_debug("Stopping distributor...\n");
    if (!_running) {
        log_error("Distributor was not running.\n");
        return;
    }

    log_debug("Disconnecting clients...\n");

    dist_header_t disconnect_request;
    disconnect_request.magic = DIST_MAGIC;
    disconnect_request.msg_type = M_DISCONNECT;

    for (infomap_t::const_iterator it = _infos.begin(); it != _infos.end(); it++) {
        const ClientInfo &c = *(it->second);
        log_debug("Sending DISCONNECT to %s:%d.\n", inet_ntoa(c.address.sin_addr), ntohs(c.address.sin_port));
        ssize_t s_ret = sendto(_fd, &disconnect_request, sizeof(dist_header_t), 0, (const struct sockaddr *) &(c.address), sizeof(struct sockaddr_in));
        if (s_ret < 0) log_error("Error sending DISCONNECT to %s:%d: %s.\n", inet_ntoa(c.address.sin_addr), ntohs(c.address.sin_port), strerror(errno));
    }

    log_debug("Closing socket...\n");
    int close_ret = close(_fd);

    if (close_ret < 0) {
        log_fatal("close(): %s\n", strerror(close_ret));
    }

    log_debug("Resetting switch...\n");
    Switch::Reset();

    log_debug("Cleaning up associations...\n");
    log_logic("Obtaining write lock...\n");
    std::lock_guard<std::mutex> lck (_write_mtx);
    log_logic("Obtained write lock.\n");
    _clients.clear();
    _infos.clear();

    // TODO: clean up threads vector

    log_info("Distributor stopped.\n");

    _running = false;
}

void UdpDistributor::Join () {
    for (std::thread &t : _threads) {
        if (t.joinable()) t.join();
    }
}

}