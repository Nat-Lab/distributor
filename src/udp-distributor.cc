#include "udp-distributor.h"
#include "log.h"
#include "vars.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace distributor {

InetSocketAddress::InetSocketAddress() {}
InetSocketAddress::InetSocketAddress(const struct sockaddr_in &addr) {
    memcpy(&address, &addr, sizeof(struct sockaddr_in));
    hash = addr.sin_addr.s_addr + (addr.sin_port << sizeof(addr.sin_addr.s_addr));
    log_logic("Hash(%s:%d): %zu\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), hash);
}

bool InetSocketAddress::operator== (const InetSocketAddress &other) const {
    return memcmp(&other, &address, sizeof(struct sockaddr_in)) == 0;
}

size_t InetSocketAddress::Hash() const {
    return hash;
}

size_t InetSocketAddressHasher::operator() (const InetSocketAddress &key) const {
    return key.Hash();
}

UdpDistributor::UdpDistributor(uint32_t n_threads, in_addr_t local_addr, in_port_t local_port) {
    _local_addr = local_addr;
    _local_port = local_port;
    _n_threads = n_threads;
    _running = false;
    _next_port = 1;
}

Client::Client (const struct sockaddr_in &address, int fd) {
    memcpy(&_address, &address, sizeof(struct sockaddr_in));
    _last_seen = _last_sent = time(NULL);
    _fd = fd;
    dist_header_t *hdr = (dist_header_t *) _send_buffer;
    hdr->magic = DIST_MAGIC;
}

const struct sockaddr_in& Client::AddrRef () const {
    return _address;
}

const struct sockaddr_in* Client::AddrPtr () const {
    return &_address;
}

ssize_t Client::Disconnect () {
    log_logic("Sending M_DISCONNECT...\n");
    return SendMsg(M_DISCONNECT);
}

ssize_t Client::Keepalive () {
    log_logic("Sending M_KEEPALIVE_REQUEST...\n");
    return SendMsg(M_KEEPALIVE_REQUEST);
}

ssize_t Client::AckKeepalive () {
    log_logic("Sending M_KEEPALIVE_RESPOND...\n");
    return SendMsg(M_KEEPALIVE_RESPOND);
}

ssize_t Client::Associate () {
    log_logic("Sending M_NEED_ASSOCIATION...\n");
    return SendMsg(M_NEED_ASSOCIATION);
}

ssize_t Client::AckAssociate () {
    log_logic("Sending M_ASSOCIATE_RESPOND...\n");
    return SendMsg(M_ASSOCIATE_RESPOND);
}

void Client::Saw () {
    _last_seen = time(NULL);
}

ssize_t Client::Write (const uint8_t *buffer, size_t size) {
    log_logic("Obtaining buffer lock...\n");
    std::lock_guard<std::mutex> lck (_buf_mutex);
    log_logic("Obtained buffer lock.\n");
    dist_header_t *hdr = (dist_header_t *) _send_buffer;
    hdr->msg_type = M_ETHERNET_FRAME;
    uint8_t *msg = _send_buffer + sizeof(dist_header_t);
    size_t pkt_sz = size + sizeof(dist_header_t);
    if (pkt_sz > DIST_CLIENT_SEND_BUFSZ) {
        log_error("Ethernet frame size too large. Max size : %d.\n", DIST_CLIENT_SEND_BUFSZ);
        return 0;
    }
    memcpy(msg, buffer, size);
    ssize_t s_ret = sendto(_fd, _send_buffer, pkt_sz, 0, (const struct sockaddr *) &_address, sizeof(struct sockaddr_in));
    if (s_ret < 0) {
        log_error("sendto(): %s.\n", strerror(errno));
    } else if ((size_t) s_ret != pkt_sz) {
        log_error("sendto() returned %zu.\n", s_ret);
    } else _last_sent = time(NULL);

    return s_ret;
}

ssize_t Client::SendMsg (msg_type_t type) {
    log_logic("Obtaining buffer lock...\n");
    std::lock_guard<std::mutex> lck (_buf_mutex);
    log_logic("Obtained buffer lock.\n");
    dist_header_t *hdr = (dist_header_t *) _send_buffer;
    hdr->msg_type = type;
    ssize_t s_ret = sendto(_fd, _send_buffer, sizeof(dist_header_t), 0, (const struct sockaddr *) &_address, sizeof(struct sockaddr_in));

    if (s_ret < 0) {
        log_error("sendto(): %s.\n", strerror(errno));
    } else if ((size_t) s_ret != sizeof(dist_header_t)) {
        log_error("sendto() returned %zu.\n", s_ret);
    } else _last_sent = time(NULL);

    return s_ret;
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

    for (uint32_t i = 0; i < _n_threads; i++) {
        log_debug("Starting worker %d...\n", i);
        _threads.push_back(std::thread(&UdpDistributor::Worker, this, i));
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

    /*dist_header_t disconnect_request;
    disconnect_request.magic = DIST_MAGIC;
    disconnect_request.msg_type = M_DISCONNECT;*/

    for (infomap_t::iterator it = _infos.begin(); it != _infos.end(); it++) {
        Client &c = *(it->second);
        log_debug("Sending DISCONNECT to %s:%d.\n", inet_ntoa(c.AddrRef().sin_addr), ntohs(c.AddrRef().sin_port));
        //ssize_t s_ret = sendto(_fd, &disconnect_request, sizeof(dist_header_t), 0, (const struct sockaddr *) c.AddrPtr(), sizeof(struct sockaddr_in));
        ssize_t s_ret = c.Disconnect();
        if (s_ret < 0) log_error("Error sending DISCONNECT to %s:%d: %s.\n", inet_ntoa(c.AddrRef().sin_addr), ntohs(c.AddrRef().sin_port), strerror(errno));
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

void UdpDistributor::Worker (int id) {
    log_debug("Worker%d: started.\n", id);
    struct sockaddr_in client_addr;
    uint8_t buffer[DIST_WOROKER_READ_BUFSZ];

    while (_running) {
        socklen_t client_addr_len = sizeof(struct sockaddr_in);

        log_logic("Worker%d: waiting for incoming packet...\n", id);
        ssize_t len = recvfrom(_fd, buffer, DIST_WOROKER_READ_BUFSZ, 0, (struct sockaddr *) &client_addr, &client_addr_len);

        log_logic("Worker%d: Packet from %s:%d.\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        if (len < 0) {
            log_error("Worker%d: recvfrom(): %s.\n", id, strerror(errno));
            break;
        }

        if (len == 0) {
            log_error("Worker%d: recvfrom() returned 0.\n", id);
            break;
        }

        if ((size_t) len < sizeof(dist_header_t)) {
            log_warn("Worker%d: received packet too small.\n", id);
            continue;
        }

        const dist_header_t *msg_hdr = (const dist_header_t *) buffer;

        if (ntohs(msg_hdr->magic) != DIST_MAGIC) {
            log_warn("Worker%d: received invalid packet from %s:%d (Invalid magic).\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            continue;
        }

        // find/create client info
        InetSocketAddress c (client_addr);
        clientsmap_t::iterator cit = _clients.find(c);
        infomap_t::iterator iit = _infos.end();
        if (cit == _clients.end()) {
            log_debug("Worker%d: Client info for %s:%d does not exist, creating...\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            log_logic("Obtaining write lock...\n");
            std::lock_guard<std::mutex> lck (_write_mtx);
            log_logic("Obtained write lock.\n");
            port_t port = _next_port++;
            std::pair<clientsmap_t::iterator, bool> clients_find_ret = _clients.insert(std::make_pair(c, port));

            cit = clients_find_ret.first;
            if (!clients_find_ret.second) {
                log_warn("Worker%d: Insert client -> port mapping returned element exist.\n", id);
            }

            std::pair<infomap_t::iterator, bool> info_find_ret = _infos.insert(std::make_pair(port, std::make_shared<Client>(client_addr, _fd)));

            iit = info_find_ret.first;
            if (!info_find_ret.second) {
                log_warn("Worker%d: Insert port -> info mapping returned element exist.\n", id);
            }
            
            log_info("Worker%d: New client from %s:%d, assigned port: %" PRIport ".\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), port);
        }

        port_t port = cit->second;
        if (iit == _infos.end()) iit = _infos.find(port);
        if (iit == _infos.end()) {
            log_warn("Worker%d: Client found in client -> port mapping but not port -> info mapping.\n", id);
            log_logic("Obtaining write lock...\n");
            std::lock_guard<std::mutex> lck (_write_mtx);
            log_logic("Obtained write lock.\n");
            std::pair<infomap_t::iterator, bool> info_find_ret = _infos.insert(std::make_pair(port, std::make_shared<Client>(client_addr, _fd)));

            iit = info_find_ret.first;
            if (!info_find_ret.second) {
                log_warn("Worker%d: Re-Insert port -> info mapping returned element exist.\n", id);
            }
        }

        size_t msg_len = (size_t) len - sizeof(dist_header_t);
        const uint8_t *msg_ptr = buffer + sizeof(dist_header_t);

        // now we have complete picture of who client is (iit & cit), process client's message
        switch (msg_hdr->msg_type) {
            case M_ETHERNET_FRAME:
                log_logic("Got M_ETHERNET_FRAME from client on port %" PRIport ".\n", port);
                if (!Forward(port, msg_ptr, msg_len)) {
                    log_info("Sending associate request to client on port %" PRIport ".\n", port);
                    iit->second->Associate();
                }
                break;
            case M_ASSOCIATE_REQUEST: {
                log_logic("Got M_ASSOCIATE_REQUEST from client on port %" PRIport ".\n", port);
                if (msg_len != sizeof(net_t)) {
                    log_warn("Invalid ASSOCIATE_REQUEST message from client on port %" PRIport ".\n", port);
                    break;
                }
                net_t net = ntohl(*(const net_t *) msg_ptr);
                log_info("Associating client on port %" PRIport " with network %" PRInet ".\n", port, net);
                Plug(net, port);
                iit->second->AckAssociate();
                break;
            }
            case M_KEEPALIVE_REQUEST: 
                log_logic("Got M_KEEPALIVE_REQUEST from client on port %" PRIport ".\n", port);
                iit->second->AckKeepalive();
                break;
            case M_KEEPALIVE_RESPOND:
                log_logic("Got M_KEEPALIVE_RESPOND from client on port %" PRIport ".\n", port);
                break;
            case M_DISCONNECT: {
                log_logic("Got M_DISCONNECT from client on port %" PRIport ".\n", port);
                log_info("Got disconnect request from client on port %" PRIport ", unregister client.\n", port);
                log_logic("Obtaining write lock...\n");
                std::lock_guard<std::mutex> lck (_write_mtx);
                log_logic("Obtained write lock.\n");
                _clients.erase(cit);
                _infos.erase(iit);
                continue;
            }
            default:
                log_warn("Invalid message type %d from client on port %" PRIport ".\n", msg_hdr->msg_type, port);
                continue;
        }

        // "continue" not called (i.e. valid msg from client, update last seen.)
        log_logic("Updating last seen for client on port %" PRIport ".\n", port);
        iit->second->Saw();

        // FIXME: what if iit/cit got deleted during message processing?
    }

    if (!_running) log_info("Worker%d: stopped.\n", id);
    else log_warn("Worker%d: stopped unexpectedly.\n", id);
}

void UdpDistributor::Send (port_t client, const uint8_t *buffer, size_t size) {
    if (!_running) {
        log_error("Send called but Distributor was not running.\n");
        return;
    }

    infomap_t::const_iterator iit = _infos.find(client);

    if (iit == _infos.end()) {
        log_error("Send called on unknow port %" PRIport ".\n", client);
        return;
    }

    iit->second->Write(buffer, size);
}

}