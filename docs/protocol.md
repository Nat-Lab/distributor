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

### Server Session FSM

#### States

- `S_IDLE`: Idle.
- `S_ASSOCIATED`: Client associated with a network.
- `S_ASSOCIATED_TIMEOUT`: Client associated with a network, but failed to respond to keepalive messages.

#### Events

- `E_STOP_REQUEST`: User stopping the server.
- `E_FORWARD`: Ethernet frame received from the client.
- `E_KEEPALIVE_REQUEST`: Keepalive request received from the client.
- `E_KEEPALIVE_RESPOND`: Keepalive responds received from the client.
- `E_ASSOCIATE_REQUEST`: Association request received from the client.
- `E_DISCONNECT_REQUEST`: Disconnect request received from the client.
- `E_IDLEING`: No packets received from the client for a user-defined amount of time.

#### Actions

- `A_FORWARD`: Forward an ethernet frame to the associated network.
- `A_REQUEST_KEEPALIVE`: Request a keepalive from the client.
- `A_RESPOND_KEEPALIVE`: Respond a keepalive to the client.
- `A_RESPOND_ASSOCIATE`: Respond to the client's association request.
- `A_NEED_ASSOCIATION`: Request an association request from the client.
- `A_SEND_DISCONNECT`: Request client to close the connection.
#### Transitions

|Current State|Event|Action to Take|New State|
|---|---|---|---|
|`S_IDLE`|`E_STOP_REQUEST`||`S_IDLE`|
|`S_IDLE`|`E_FORWARD`|`A_NEED_ASSOCIATION`|`S_IDLE`|
|`S_IDLE`|`E_KEEPALIVE_REQUEST`|`A_RESPOND_KEEPALIVE`|`S_IDLE`|
|`S_IDLE`|`E_KEEPALIVE_RESPOND`||`S_IDLE`|
|`S_IDLE`|`E_ASSOCIATE_REQUEST`|`A_RESPOND_ASSOCIATE`|`S_ASSOCIATED`|
|`S_IDLE`|`E_IDLEING`||`S_IDLE`|
|`S_ASSOCIATED`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_ASSOCIATED`|`E_STOP_REQUEST`|`A_SEND_DISCONNECT`|`S_IDLE`|
|`S_ASSOCIATED`|`E_FORWARD`|`A_FORWARD`|`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_KEEPALIVE_REQUEST`|`A_RESPOND_KEEPALIVE`|`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_KEEPALIVE_RESPOND`||`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_ASSOCIATE_REQUEST`|`A_RESPOND_ASSOCIATE`|`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_ASSOCIATED`|`E_IDLEING`|`A_REQUEST_KEEPALIVE`|`S_ASSOCIATED_TIMEOUT`|
|`S_ASSOCIATED_TIMEOUT`|`E_STOP_REQUEST`|`A_SEND_DISCONNECT`|`S_IDLE`|
|`S_ASSOCIATED_TIMEOUT`|`E_FORWARD`|`A_FORWARD`|`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_KEEPALIVE_REQUEST`|`A_RESPOND_KEEPALIVE`|`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_KEEPALIVE_RESPOND`||`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_ASSOCIATE_REQUEST`|`A_RESPOND_ASSOCIATE`|`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_ASSOCIATED_TIMEOUT`|`E_IDLEING`|`A_SEND_DISCONNECT`|`S_IDLE`|

### Client Session FSM

#### States

- `S_IDLE`: Idle.
- `S_CONNECTE`: Try connecting to the server.
- `S_CONNECTED`: Connected to the server.
- `S_ASSOCIATED`: Connected to the server and associated with a network.
- `S_ASSOCIATED_TIMEOUT`: Connected with the server and associated with a network, but the server failed to respond to keepalive messages.

#### Events

- `E_START_REQUEST`: User requested the client to start.
- `E_STOP_REQUEST`: User requested the the client to stop.
- `E_FORWARD`: Ethernet framed arrived and it needs to be deliver to the network.
- `E_KEEPALIVE_REQUEST`: Got a keepalive request from the server.
- `E_KEEPALIVE_RESPOND`: Got a keepalive respond from the server.
- `E_ASSOCIATE_RESPOND`: Got association acknowledgement from the server.
- `E_NEED_ASSOCIATION`: Got association request from the server.
- `E_DISCONNECT_REQUEST`: Got disconnect request from the server.
- `E_IDLEING`: No packets received from the server for a user-defined amount of time.

#### Actions

- `A_FORWARD`: Send the ethernet frame to the server.
- `A_REQUEST_KEEPALIVE`: Request a keepalive message from the server.
- `A_RESPOND_KEEPALIVE`: Respond to keepalive message.
- `A_REQUEST_ASSOCIATE`: Request to associate with a network.
- `A_SEND_DISCONNECT`: Request to disconnect.

#### Transitions

|Current State|Event|Action to Take|New State|
|---|---|---|---|
|`S_IDLE`|`E_START_REQUEST`|`A_REQUEST_KEEPALIVE`|`S_CONNECT`|
|`S_IDLE`|`E_STOP_REQUEST`||`S_IDLE`|
|`S_IDLE`|`E_FORWARD`|`A_REQUEST_KEEPALIVE`|`S_CONNECT`|
|`S_IDLE`|`E_KEEPALIVE_REQUEST`||`S_IDLE`|
|`S_IDLE`|`E_KEEPALIVE_RESPOND`||`S_IDLE`|
|`S_IDLE`|`E_ASSOCIATE_RESPOND`||`S_IDLE`|
|`S_IDLE`|`E_NEED_ASSOCIATION`||`S_IDLE`|
|`S_IDLE`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_IDLE`|`E_IDLEING`||`S_IDLE`|
|`S_CONNECT`|`E_START_REQUEST`||`S_CONNECT`|
|`S_CONNECT`|`E_STOP_REQUEST`|`A_SEND_DISCONNECT`|`S_IDLE`|
|`S_CONNECT`|`E_FORWARD`||`S_CONNECT`|
|`S_CONNECT`|`E_KEEPALIVE_REQUEST`|`A_RESPOND_KEEPALIVE`|`S_CONNECT`|
|`S_CONNECT`|`E_KEEPALIVE_RESPOND`||`S_CONNECT`|
|`S_CONNECT`|`E_ASSOCIATE_RESPOND`||`S_CONNECT`|
|`S_CONNECT`|`E_NEED_ASSOCIATION`||`S_IDLE`|
|`S_CONNECT`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_CONNECT`|`E_IDLEING`|`A_REQUEST_KEEPALIVE`|`S_CONNECT`|
|`S_CONNECTED`|`E_START_REQUEST`||`S_CONNECTED`|
|`S_CONNECTED`|`E_STOP_REQUEST`|`A_SEND_DISCONNECT`|`S_IDLE`|
|`S_CONNECTED`|`E_FORWARD`||`S_CONNECTED`|
|`S_CONNECTED`|`E_KEEPALIVE_REQUEST`|`E_KEEPALIVE_RESPOND`|`S_CONNECTED`|
|`S_CONNECTED`|`E_KEEPALIVE_RESPOND`||`S_CONNECTED`|
|`S_CONNECTED`|`E_ASSOCIATE_RESPOND`||`S_ASSOCIATED`|
|`S_CONNECTED`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_CONNECTED`|`E_IDLEING`|`A_REQUEST_KEEPALIVE`|`S_CONNECT`|
|`S_ASSOCIATED`|`E_START_REQUEST`||`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_STOP_REQUEST`|`A_SEND_DISCONNECT`|`S_IDLE`|
|`S_ASSOCIATED`|`E_FORWARD`|`A_FORWARD`|`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_KEEPALIVE_REQUEST`|`E_KEEPALIVE_RESPOND`|`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_KEEPALIVE_RESPOND`||`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_ASSOCIATE_RESPOND`||`S_ASSOCIATED`|
|`S_ASSOCIATED`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_ASSOCIATED`|`E_IDLEING`|`A_REQUEST_KEEPALIVE`|`S_ASSOCIATED_TIMEOUT`|
|`S_ASSOCIATED_TIMEOUT`|`E_START_REQUEST`||`S_ASSOCIATED_TIMEOUT`|
|`S_ASSOCIATED_TIMEOUT`|`E_STOP_REQUEST`|`A_SEND_DISCONNECT`|`S_IDLE`|
|`S_ASSOCIATED_TIMEOUT`|`E_FORWARD`|`A_FORWARD`|`S_ASSOCIATED_TIMEOUT`|
|`S_ASSOCIATED_TIMEOUT`|`E_KEEPALIVE_REQUEST`|`A_RESPOND_KEEPALIVE`|`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_KEEPALIVE_RESPOND`||`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_ASSOCIATE_RESPOND`||`S_ASSOCIATED`|
|`S_ASSOCIATED_TIMEOUT`|`E_DISCONNECT_REQUEST`||`S_IDLE`|
|`S_ASSOCIATED_TIMEOUT`|`E_IDLEING`|`A_REQUEST_KEEPALIVE`|`S_CONNECT`|
