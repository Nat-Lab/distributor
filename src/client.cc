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
    fprintf(stderr, "usage: %s [-h] -d DEV -s SERVER_ADDR -p SERVER_PORT\n", me);
    fprintf(stderr, "\n");
    fprintf(stderr, "TUN/TAP based Linux client for distributor.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "required arguments:\n");
    fprintf(stderr, "  -d DEV           Name of the TAP device to create.\n");
    fprintf(stderr, "  -s SERVER_ADDR   IP address of the distributor.\n");
    fprintf(stderr, "  -p SERVER_PORT   Port of the distributor.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "optional arguments:\n");
    fprintf(stderr, "  -h               Print this help message and exit.\n");
}

int main (int argc, char **argv) {
    char opt;
    char *dev = nullptr;
    char *server = nullptr;
    in_port_t port = 0;

    while ((opt = getopt(argc, argv, "hd:s:p:")) != -1) {
        switch (opt) {
            case 'd': 
                dev = strdup(optarg);
                break;
            case 's':
                server = strdup(optarg);
                break;
            case 'p':
                port = (in_port_t) atoi(optarg);
                break;
            case 'h':
                help (argv[0]);
                return 0;
            default:
                help (argv[0]);
                return 1;
        }
    }

    if (dev == nullptr || server == nullptr || port == 0) {
        help (argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    TapClient client (dev, strlen(dev), inet_addr(server), htons(port), 1);
    ::client = &client;
    client.Start();
    client.Join();

    free(dev);
    free(server);
    return 0;
} 