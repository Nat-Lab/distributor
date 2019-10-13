#include "tap-client.h"
#include "log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>

using namespace distributor;

static TapClient *client = nullptr;

void handle_signal (__attribute__((unused)) int sig) {
    if (client != nullptr) {
        log_info("Got SIGINT/SIGTERM, stopping...\n");
        client->Stop();
        exit(0);
    }
}

void help (const char *me) {
    fprintf(stderr, "usage: %s [-h] [-m MTU] -d DEV -s SERVER_ADDR -p SERVER_PORT -n NET\n", me);
    fprintf(stderr, "\n");
    fprintf(stderr, "TUN/TAP based Linux client for distributor.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "required arguments:\n");
    fprintf(stderr, "  -d DEV           Name of the TAP device to create.\n");
    fprintf(stderr, "  -s SERVER_ADDR   IP address of the distributor.\n");
    fprintf(stderr, "  -p SERVER_PORT   Port of the distributor.\n");
    fprintf(stderr, "  -n NET           ID of the network to join.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "optional arguments:\n");
    fprintf(stderr, "  -h               Print this help message and exit.\n");
    fprintf(stderr, "  -m MTU           Set MTU for TAP interface. (default: 1400, use multiple of\n");
    fprintf(stderr, "                   1400 for best performance)\n");
}

int main (int argc, char **argv) {
    char opt;
    char *dev = nullptr;
    char *server = nullptr;
    in_port_t port = 0;
    net_t net = 0;
    int mtu = 1400;
    bool net_set = false;

    while ((opt = getopt(argc, argv, "hm:d:s:p:n:")) != -1) {
        switch (opt) {
            case 'm':
                mtu = atoi(optarg);
                continue;
            case 'd': 
                dev = strdup(optarg);
                continue;
            case 's':
                server = strdup(optarg);
                continue;
            case 'p':
                port = (in_port_t) atoi(optarg);
                continue;
            case 'n':
                net = (net_t) atoi(optarg);
                net_set = true;
                continue;
            case 'h':
                help (argv[0]);
                return 0;
            default:
                help (argv[0]);
                return 1;
        }
    }

    if (dev == nullptr || server == nullptr || port == 0 || !net_set) {
        help (argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    TapClient client (dev, strlen(dev), mtu, inet_addr(server), htons(port), net);
    ::client = &client;
    client.Start();
    client.Join();

    free(dev);
    free(server);
    return 0;
} 