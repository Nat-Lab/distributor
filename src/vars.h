#ifndef DIST_VARS_H
#define DIST_VARS_H

// mac address ageing time in second in fdb. 
#ifndef DIST_FDB_AGEING 
#define DIST_FDB_AGEING 300
#endif 

// interval of the keepalive.
#ifndef DIST_FDB_AGEING
#define DIST_UDP_KEEPALIVE 60
#endif

// remove clients after N times they failed to respond to keepalive
#ifndef DIST_UDP_RETRIES
#define DIST_UDP_RETRIES 3
#endif

#endif // DIST_VARS_H