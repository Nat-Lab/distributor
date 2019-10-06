#include "udp-distributor.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace distributor;

static UdpDistributor *dist = nullptr;

void handle_signal (__attribute__((unused)) int sig) {
    if (dist != nullptr) {
        log_info("Got SIGINT/SIGTERM, stopping...\n");
        dist->Stop();
        exit(0);
    }
}

void help (const char *me) {
    fprintf(stderr, "usage: %s [-h] -b BIND_ADDR -p BIND_PORT\n", me);
    fprintf(stderr, "\n");
    fprintf(stderr, "TUN/TAP based Linux client for distributor.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "required arguments:\n");
    fprintf(stderr, "  -b BIND_ADDR     Address to bind on.\n");
    fprintf(stderr, "  -p BIND_PORT     Port to run on.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "optional arguments:\n");
    fprintf(stderr, "  -h               Print this help message and exit.\n");
}

int main (int argc, char **argv) {
    char opt;
    char *bind_addr = nullptr;
    in_port_t port = 0;

    while ((opt = getopt(argc, argv, "hb:p:")) != -1) {
        switch (opt) {
            case 'b':
                bind_addr = strdup(optarg);
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

    if (bind_addr == nullptr || port == 0) {
        help (argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    UdpDistributor dist (inet_addr(bind_addr), htons(port));
    ::dist = &dist;
    dist.Start();
    dist.Join();

    free(bind_addr);
    return 0;
} 