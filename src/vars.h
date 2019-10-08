#ifndef DIST_VARS_H
#define DIST_VARS_H

// mac address ageing time in second in fdb.  
#define DIST_FDB_AGEING 300

// interval of the keepalive.
#define DIST_UDP_KEEPALIVE 60

// remove clients after N times they failed to respond to keepalive
#define DIST_UDP_RETRIES 3

#endif // DIST_VARS_H