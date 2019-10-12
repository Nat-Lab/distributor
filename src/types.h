#ifndef DIST_TYPES_H
#define DIST_TYPES_H
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>

#define PRIport PRIu64
#define PRInet PRIu32

namespace distributor {

typedef ::uint64_t port_t;
typedef ::uint32_t net_t;

struct dist_header {
    uint16_t magic;
    uint8_t msg_type;
} __attribute__ ((__packed__));

typedef struct dist_header dist_header_t;

enum msg_type {
    M_ETHERNET_FRAME = 0, 
    M_ASSOCIATE_REQUEST = 1, 
    M_ASSOCIATE_RESPOND = 2,
    M_KEEPALIVE_REQUEST = 3, 
    M_KEEPALIVE_RESPOND = 4,
    M_NEED_ASSOCIATION = 5,
    M_DISCONNECT = 6
};

typedef msg_type msg_type_t;

}

#endif // DIST_TYPES_H