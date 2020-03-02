distributor
---

`distributor` is a simple virtual ethernet switch that supports up to `2^32` networks. 

### Installation

Clone this repository and build it with `make`. 

```
$ git clone https://github.com/nat-lab/distributor/
$ cd distributor
$ make
```

After compilation, you will find `distributor` and `tap-client`, which are the distributor server and the TAP-based Linux client, respectively. Type `./distributor -h` and `./tap-client -h` to get help on how to use them.

### Development

Protocol specifications can be found under the `doc/` folder. If you don't care about the protocol but simply want to build your own client, take a look at `src/fd-client.h` and `src/fd-client.cc`. `FdClient` provides you with a file descriptor similar to TUN/TAP, that you can write to or read from to get ethernet traffic on and oof the virtual network.
