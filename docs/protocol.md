Protocol Specification
---

### Packet Format

```
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             MAGIC             |    MSG_TYPE   |  MSG_PAYLOAD  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

MAGIC       : Protocol identifier, 2 bytes integer. 0x5EED, network bytes.
MSG_TYPE    : Type of the MSG_PAYLOAD, 1 byte unsigned integer.
MSG_PAYLOAD : Message body, variable length.

where MSG_TYPE = { 
    ETHERNET_FRAME = 0, 
    ASSOCIATE_REQUEST = 1, 
    ASSOCIATE_RESPOND = 2,
    KEEPALIVE_REQUEST = 3, 
    KEEPALIVE_RESPOND = 4,
    NEED_ASSOCIATION = 5,
    DISCONNECT = 6
}
```

### Message Format

- `ETHERNET_FRAME`: Standard ethernet frame, variable length.
- `ASSOCIATE_REQUEST`: 4 bytes unsigned integer in network byte order.
- `ASSOCIATE_RESPOND`: No payload.
- `KEEPALIVE_REQUEST`: No payload.
- `KEEPALIVE_RESPOND`: No payload.
- `NEED_ASSOCIATION`: No payload.
- `DISCONNECT`: No payload.

### Message Usage

- `ETHERNET_FRAME`: Ethernet frame.
- `ASSOCIATE_REQUEST`: Send by the client to server, to request the association with a network.
- `ASSOCIATE_RESPOND`: Send by the server to the client to acknowledge the client's association request.
- `KEEPALIVE_REQUEST`: Send by client or server to the other side, to request a `KEEPALIVE_RESPOND` from the other side. This should be used to initialize connection before `ASSOCIATE_REQUEST`
- `KEEPALIVE_RESPOND`: Reply to `KEEPALIVE_RESPOND`.
- `NEED_ASSOCIATION`: Send by the server to clients, to request the client to send an `ASSOCIATE_REQUEST` message.
- `DISCONNECT`: Send by client or server to the other side, to request the other side to close the connection. 