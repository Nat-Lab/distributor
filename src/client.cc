#include "client.h"
#include "log.h"
#include <unistd.h>

namespace distributor {

DistributorClient::DistributorClient (in_addr_t server_addr, in_port_t port, net_t net) {
    memset(&_server, 0, sizeof(struct sockaddr_in));
    _server.sin_addr.s_addr = server_addr;
    _server.sin_family = AF_INET;
    _server.sin_port = port;
    _running = false;
    _state = S_IDLE;
}

void DistributorClient::SetNetwork (net_t net) {
    static uint8_t buffer[sizeof(dist_header_t) + sizeof(net_t)];
    _net = net;
    if (_running && _state > S_CONNECTE) {
        dist_header_t *hdr = (dist_header_t *) buffer;
        hdr->magic = DIST_CLIENT_MAGIC;
        hdr->msg_type = M_ASSOCIATE_REQUEST;
        uint8_t *msg_ptr = buffer + sizeof(dist_header_t);
        *msg_ptr = htonl(net);
        sendto(_fd, hdr, sizeof(dist_header_t) + sizeof(net_t), 0, (const struct sockaddr *) &_server, sizeof(struct sockaddr_in));
    }
}

void DistributorClient::Start () {
    if (_running) {
        log_error("Already running.\n");
        return;
    }

    _fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_fd < 0) {
        log_fatal("socket(): %s\n", strerror(errno));
        return;
    }

    _running = true;
    _state = S_IDLE;

    NicStart();

    _threads.push_back(std::thread(&DistributorClient::SocketWorker, this));
    _threads.push_back(std::thread(&DistributorClient::NicWorker, this));
    _threads.push_back(std::thread(&DistributorClient::Pinger, this));

    log_info("Clinet ready.\n");
}

void DistributorClient::Stop () {
    if (!_running) {
        log_error("Not running.\n");
        return;
    }

    log_debug("Request Disconnect socket...\n");
    SendMsg(M_DISCONNECT);

    log_debug("Closing socket...\n");
    close(_fd);

    NicStop();

    _running = false;
    _state = S_IDLE;
}


}