We write the implementation of a simple chat server.

`common.c` - implementations for sending and receiving a `struct chat_packet`

`server.c` - server code

`client.c` - client code

We use the following API today:

```c
// Create an endpoint for communication (usually on the network): the socket is
// modelled as a file descriptor.
int socket (int domain, int type, int protocol);

// Assign the address specified by addr to the socket referred to by sockfd.
// Other parties will be able to connect to this socket on the specified address.
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// Connect the socket referred to by the file descriptor sockfd to the address
// specified by addr.
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// Marks the socket referred to by sockfd as a passive socket (a socket that will
// be used to accept incoming connection requests using accept).
int listen(int sockfd, int backlog);

// When a connection request comes in on the listening socket referred to by
// sockfd, creates a new connected socket, and return a new file descriptor
// referring to that socket. The newly created socket can be used to communicate
// with the party which made the connection request.
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Send at most len bytes of buf to socket sockfd. Returns the number sent or
// -1. For TCP sockets, this function returns when len bytes of buf have been
// accepted by the TCP/IP stack, and the stack guarantees they will be sent.
ssize_t send(int sockfd, const void *buf, size_t len, int flags);

// Receive at most len bytes from sockfd into buf. This function blocks (if
// socket is not nonblocking) until some data is available on sockfd, and may
// return less bytes than len. The function returns how many bytes it wrote into
// buf.
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

We use the following structure for the messages exchanged by the server and the
clients:

```c
#define MSG_MAXSIZE 1024

struct chat_packet {
  uint16_t len;
  char message[MSG_MAXSIZE + 1];
};
```

## Topology

```
                    |‾‾‾‾‾‾‾‾|
client1 -- (r-eth1) | Router | (r-eth2) -- client2
                    |________|
                     (r-eth0)
                        ¦
                      server

Interfaces

r-eth0: 192.168.0.1/24
r-eth1: 192.168.1.1/24
r-eth2: 192.168.2.1/24
```
## Usage

To compile the code
```
make
```

To start the topology (terminals for each device will be started as well):
```bash
sudo pkill ovs-test
sudo python3 topo.py
```

To start the `server`, in the `server` device, simply run `./server 192.168.0.2 [PORT]`.

To start a client, in a `client#` device, simply run `./client 192.168.0.2 [PORT]`.
# ChatApp
