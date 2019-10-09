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

}

#endif // DIST_TYPES_H