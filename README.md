### Copyright 2024 Toma-Ioan Dumitrescu


### Description

The project represents a chat app on the model client-server, using TCP protocol,
mainly offering the following feature: one user sends a message, and that message is
displayed to all clients (together with the username) connected to the server, just
like in a WhatsApp group. Using I/ O multiplexing with poll(), the messages are sent/
received in real time, the server accepts new clients at any moment, and there is no
restriction in the order of the user communications. The implementations of send_all
and recv_all will avoid truncations or concatenations, by using a loop that tracks
exactly how many bytes are sent or received after a system call.

Other features: at every 10 seconds, the server sends to all clients (or to the group)
a promotional message; just like in a WhatsApp group, chat history is memorized, and
the server can perform a multithreading search over all messages and get all matches.

### Topology

It is created using Python and Mininet: N clients connected to a router with N + 1 eth
interfaces, the router communicating with the server.

Example of a command to test the program:
sudo python3 topo.py --clients 7
