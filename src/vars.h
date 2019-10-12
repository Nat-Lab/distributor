#ifndef DIST_VARS_H
#define DIST_VARS_H

// mac address ageing time in second in fdb. 
#ifndef DIST_FDB_AGEING 
#define DIST_FDB_AGEING 300
#endif // DIST_FDB_AGEING

// interval of the keepalive.
#ifndef DIST_UDP_KEEPALIVE
#define DIST_UDP_KEEPALIVE 60
#endif // DIST_UDP_KEEPALIVE

// remove clients after N times they failed to respond to keepalive
#ifndef DIST_UDP_RETRIES
#define DIST_UDP_RETRIES 3
#endif

#ifndef DIST_WOROKER_READ_BUFSZ
#define DIST_WOROKER_READ_BUFSZ 65536
#endif // DIST_WOROKER_READ_BUFSZ

#ifndef DIST_CLIENT_SEND_BUFSZ
#define DIST_CLIENT_SEND_BUFSZ 65536
#endif // DIST_CLIENT_SEND_BUFSZ

#endif // DIST_VARS_H