#include "client.h"
#include "log.h"
#include <unistd.h>
#include <arpa/inet.h>

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
    log_debug("Setting network to %" PRInet ".\n", net);
    static uint8_t buffer[sizeof(dist_header_t) + sizeof(net_t)];
    _net = net;

    if (_running && _state >= S_CONNECTED) {
        log_debug("Client is already connect with server, send association request.\n");
        dist_header_t *hdr = (dist_header_t *) buffer;
        hdr->magic = DIST_CLIENT_MAGIC;
        hdr->msg_type = M_ASSOCIATE_REQUEST;
        uint8_t *msg_ptr = buffer + sizeof(dist_header_t);
        *msg_ptr = htonl(net);
        ssize_t s_ret = sendto(_fd, hdr, sizeof(dist_header_t) + sizeof(net_t), 0, (const struct sockaddr *) &_server, sizeof(struct sockaddr_in));
        if (s_ret < 0) {
            log_error("sendto(): %s.\n", strerror(errno));
            return;
        }
        if ((size_t) s_ret != sizeof(dist_header_t) + sizeof(net_t)) {
            log_error("sendto() returned %zu.\n", s_ret);
            return;
        }
        _last_sent = time(NULL);
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
    _state = S_CONNECT;

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

void DistributorClient::Join () {
    for (std::thread &t : _threads) {
        if (t.joinable()) t.join();
    }
}

ssize_t DistributorClient::SendMsg (msg_type_t type) {
    if (!_running) {
        log_error("Client not running.\n");
        return 0;
    }
    static dist_header_t msg;
    msg.magic = DIST_CLIENT_MAGIC;
    msg.msg_type = type;
    ssize_t s_ret = sendto(_fd, &msg, sizeof(dist_header_t), 0, (const struct sockaddr *) &_server, sizeof(struct sockaddr_in));
    if (s_ret < 0) {
        log_error("sendto(): %s.\n", strerror(errno));
        return s_ret;
    }
    if ((size_t) s_ret != sizeof(dist_header_t) + sizeof(net_t)) {
        log_error("sendto() returned %zu.\n", s_ret);
        return s_ret;
    }
    _last_sent = time (NULL);
    return s_ret;
}

void DistributorClient::SocketWorker () {
    log_debug("Socket worker started.\n");
    uint8_t buffer[DIST_CLIENT_BUF_SZ];
    struct sockaddr_in recv_addr;
    while (_running) {
        static socklen_t saddr_len = sizeof(struct sockaddr_in);
        ssize_t len = recvfrom(_fd, buffer, DIST_CLIENT_BUF_SZ, 0, (struct sockaddr *) &recv_addr, &saddr_len);
        _last_recv = time(NULL);

        if (len < 0) {
            log_error("recvfrom(): %s.\n", strerror(errno));
            continue;
        }

        if (len == 0) {
            log_error("recvfrom() returned 0.\n");
            continue;
        }

        if (recv_addr.sin_addr.s_addr != _server.sin_addr.s_addr && recv_addr.sin_port != _server.sin_port) {
            log_warn("Got packet from invalid source %s:%d.\n", inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port));
            continue;
        }

        if ((size_t) len < sizeof(dist_header_t)) {
            log_warn("Received packet too small.\n");
            continue;
        }

        const dist_header_t *msg_hdr = (const dist_header_t *) buffer;

        if (ntohs(msg_hdr->magic) != DIST_CLIENT_MAGIC) {
            log_warn("received invalid packet from %s:%d (Invalid magic).\n", inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port));
            continue;
        }

        size_t msg_len = (size_t) len - sizeof(dist_header_t);
        uint8_t *msg_ptr = buffer + sizeof(dist_header_t);

        switch (_state) {
            case S_IDLE: {
                log_warn("Packet received in IDLE state.\n");
                continue; // got pkt in IDLE, noting to do.
            }
            case S_CONNECT: {
                log_logic("Packet received in CONNECT state.\n");
                switch (msg_hdr->msg_type) {
                    case M_KEEPALIVE_REQUEST: {
                        log_logic("Packet type is KEEPALIVE_REQUEST, respond.\n");
                        SendMsg(M_KEEPALIVE_RESPOND);
                        continue;
                    }
                    case M_KEEPALIVE_RESPOND: {
                        log_info("Connected to server, associating with network %" PRInet "...\n", _net);
                        _state = S_CONNECTED;
                        SetNetwork(_net);
                        continue;
                    }
                    case M_DISCONNECT: {
                        log_info("Got disconnect request from server, go to IDLE.\n");
                        _state = S_IDLE;
                        continue;
                    }
                    default: {
                        log_warn("Out-of-context message of type %d received in CONNECT state.\n", msg_hdr->msg_type);
                        continue;
                    }
                }
                break;
            }
            case S_CONNECTED: {
                log_logic("Packet received in CONNECTED state.\n");
                switch (msg_hdr->msg_type) {
                    case M_KEEPALIVE_REQUEST: {
                        log_logic("Packet type is KEEPALIVE_REQUEST, respond.\n");
                        SendMsg(M_KEEPALIVE_RESPOND);
                        continue;
                    }
                    case M_KEEPALIVE_RESPOND: {
                        log_logic("Packet type is KEEPALIVE_RESPOND.\n");
                        continue;
                    }
                    case M_DISCONNECT: {
                        log_info("Got disconnect request from server, go to IDLE.\n");
                        _state = S_IDLE;
                        continue;
                    }
                    case M_ASSOCIATE_RESPOND: {
                        log_info("Got association ACK from server, network connected and ready.\n");
                        _state = S_ASSOCIATED;
                        continue;
                    }
                    default: {
                        log_warn("Out-of-context message of type %d received in CONNECT state.\n", msg_hdr->msg_type);
                        continue;
                    }
                }
                break;
            }
            case S_ASSOCIATED: {
                log_logic("Packet received in ASSOCIATED state.\n");
                switch (msg_hdr->msg_type) {
                    case M_KEEPALIVE_REQUEST: {
                        log_logic("Packet type is KEEPALIVE_REQUEST, respond.\n");
                        SendMsg(M_KEEPALIVE_RESPOND);
                        continue;
                    }
                    case M_KEEPALIVE_RESPOND: {
                        log_logic("Packet type is KEEPALIVE_RESPOND.\n");
                        continue;
                    }
                    case M_DISCONNECT: {
                        log_info("Got disconnect request from server, go to IDLE.\n");
                        _state = S_IDLE;
                        continue;
                    }
                    case M_ETHERNET_FRAME: {
                        log_logic("Got ethernet frame from server.\n");
                        NicWrite(msg_ptr, msg_len);
                        continue;
                    }
                    case M_NEED_ASSOCIATION: {
                        log_info("Server requested client to re-associate.\n");
                        _state = S_CONNECTED;
                        SetNetwork(_net);
                        continue;
                    }
                    default: {
                        log_warn("Out-of-context message of type %d received in CONNECT state.\n", msg_hdr->msg_type);
                        continue;
                    }
                }
                break;
            }
        }

        log_fatal("??? Got to unreachable location.\n");
    }
    log_debug("Socket worker stopped.\n");
}

void DistributorClient::NicWorker () {
    log_debug("NIC worker started.\n");
    uint8_t buffer[DIST_CLIENT_BUF_SZ];
    dist_header_t *msg_hdr = (dist_header_t *) buffer;
    uint8_t *msg_ptr = buffer + sizeof(dist_header_t);
    msg_hdr->magic = DIST_CLIENT_MAGIC;
    msg_hdr->msg_type = M_ETHERNET_FRAME;
    size_t max_frame_len = DIST_CLIENT_BUF_SZ - sizeof(dist_header_t);

    while (_running) {
        ssize_t read_len = NicRead(msg_ptr, max_frame_len);
        if (read_len == 0) {
            log_warn("Reading from NIC returned 0. Is NIC up?\n");
            continue;
        }
        if (read_len < 0) {
            log_error("Error reading NIC: %s.\n", strerror(errno));
            continue;
        }
        if (_state != S_ASSOCIATED) {
            log_notice("Discared ethernet frame from NIC because client is not yet associated.\n");
            continue;
        }
        size_t pkt_len = sizeof(dist_header_t) + (size_t) read_len;
        ssize_t s_ret = sendto(_fd, buffer, pkt_len, 0, (const struct sockaddr *) &_server, sizeof(struct sockaddr_in));
        if (s_ret < 0) {
            log_error("sendto(): %s.\n", strerror(errno));
            return;
        }
        if ((size_t) s_ret != pkt_len) {
            log_error("sendto() returned %zu, but pkt len is %zu.\n", (size_t) s_ret, pkt_len);
            return;
        }
        _last_sent = time(NULL);

    }

    log_debug("NIC worker stopped.\n");
}

void DistributorClient::Pinger () {
    log_debug("Pinger started.\n");
    while (_running) {
        std::unique_lock<std::mutex> lock (_pinger_mtx);
        if (_state == S_IDLE) {
            log_debug("Client running but idle, send init keepalive to server.\n");
            SendMsg(M_KEEPALIVE_REQUEST);
        } else {
            int64_t lastsent_diff = time(NULL) - _last_sent;
            int64_t lastrecv_diff = time(NULL) - _last_recv;

            if (lastsent_diff >= DIST_CLIENT_KEEPALIVE && lastrecv_diff >= DIST_CLIENT_KEEPALIVE) {
                log_debug("Nothing received from server for %" PRIi64 " seconds, send keepalive.\n");
                SendMsg(M_KEEPALIVE_REQUEST);
            }

            if (lastrecv_diff >= DIST_CLIENT_RETRY * DIST_CLIENT_KEEPALIVE) {
                log_warn("Nothing received from server for %" PRIi64 " seconds, disconnect and go idle.\n");
                SendMsg(M_DISCONNECT);
                _state = S_IDLE;
            }
        }
        if (_pinger_cv.wait_for(lock, std::chrono::seconds(1)) != std::cv_status::timeout) break;
    }
    // FIXME: race-condition on SendMsg()?
    log_debug("Pinger stopped.\n");
}

}