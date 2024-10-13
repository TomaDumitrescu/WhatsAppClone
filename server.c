#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/timerfd.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32

// Receiving data on connfd1 and sending the message on connfd2
int receive_and_send(int connfd1, int connfd2, size_t len) {
  int bytes_received;
  char buffer[len];

  // Receiving exactly len bytes from connfd1
  bytes_received = recv_all(connfd1, buffer, len);
  // The connection was closed
  if (bytes_received == 0)
    return 0;
  DIE(bytes_received < 0, "recv");

  // Send the message to connfd2
  int rc = send_all(connfd2, buffer, len);
  if (rc <= 0) {
    perror("send_all");
    return -1;
  }

  return bytes_received;
}

void run_chat_server(int listenfd) {
  struct sockaddr_in client_addr1;
  struct sockaddr_in client_addr2;
  socklen_t clen1 = sizeof(client_addr1);
  socklen_t clen2 = sizeof(client_addr2);

  int connfd1 = -1;
  int connfd2 = -1;
  int rc;

  // Set listenfd socket for listening
  rc = listen(listenfd, 2);
  DIE(rc < 0, "listen");

  // Accepting two connections
  printf("Waiting for the client 1 connection...\n");
  connfd1 = accept(listenfd, (struct sockaddr *)&client_addr1, &clen1);
  DIE(connfd1 < 0, "accept");

  printf("Waiting for the client 2 connection...\n");
  connfd2 = accept(listenfd, (struct sockaddr *)&client_addr2, &clen2);
  DIE(connfd2 < 0, "accept");

  while (true) {
    printf("Receiving from 1, sending to 2...\n");
    int rc = receive_and_send(connfd1, connfd2, sizeof(struct chat_packet));
    if (rc <= 0)
      break;

    printf("Receiving from 2, sending to 1...\n");
    rc = receive_and_send(connfd2, connfd1, sizeof(struct chat_packet));
    if (rc <= 0)
      break;
  }

  // Closing the connections and the created sockets
  close(connfd1);
  close(connfd2);
}

void run_chat_multi_server(int listenfd) {

  struct pollfd poll_fds[MAX_CONNECTIONS];
  int num_sockets = 2;
  int rc;

  struct chat_packet received_packet;

  // Set listenfd socket for listening
  rc = listen(listenfd, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // Add the file descriptor in the poll set
  poll_fds[0].fd = listenfd;
  poll_fds[0].events = POLLIN;

  int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
  DIE(timerfd < 0, "timerfd_create");

  // Timer which will print a message every 10 seconds
  struct itimerspec timer;
  timer.it_value.tv_sec = 10;
  timer.it_value.tv_nsec = 0;
  timer.it_interval.tv_sec = 10;
  timer.it_interval.tv_nsec = 0;

  rc = timerfd_settime(timerfd, 0, &timer, NULL);
  DIE(rc < 0, "timerfd_settime error");

  // Adding timerfd to the poll set
  poll_fds[1].fd = timerfd;
  poll_fds[1].events = POLLIN;

  while (1) {
    // Waiting to receive something from one socket
    rc = poll(poll_fds, num_sockets, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_sockets; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == listenfd) {
          // Connection request from the listenfd socket
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          const int newsockfd =
              accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");

          // Add the new socket to the poll set
          poll_fds[num_sockets].fd = newsockfd;
          poll_fds[num_sockets].events = POLLIN;
          num_sockets++;

          printf("New connection from %s, port %d, socket client %d\n",
                 inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
                 newsockfd);
        } else if (poll_fds[i].fd == timerfd) {
            uint64_t exp;

            // Reset the timer
            read(timerfd, &exp, sizeof(exp));

            struct chat_packet ad;
            char ad_msg[50] = "Server: For 2.99$ per month, you can disable ads\n\0";
            ad.len = strlen(ad_msg);
            strcpy(ad.message, ad_msg);

            // Send the ad to all clients
            for (int j = 1; j < num_sockets; j++) {
                if (poll_fds[j].fd != timerfd && poll_fds[j].fd != listenfd) {
                    send_all(poll_fds[j].fd, &ad, sizeof(ad));
                }
            }
        } else {
          // Received data from one of the sockets
          int rc = recv_all(poll_fds[i].fd, &received_packet,
                            sizeof(received_packet));
          DIE(rc < 0, "recv");

          if (rc == 0) {
            printf("Socket-ul client %d a inchis conexiunea\n", i);
            close(poll_fds[i].fd);

            // Delete the reading element from the poll set
            for (int j = i; j < num_sockets - 1; j++) {
              poll_fds[j] = poll_fds[j + 1];
            }

            num_sockets--;
          } else {
            printf("Received a packet from the client with socket %d, having the message: %s\n",
                    poll_fds[i].fd, received_packet.message);

            for (int j = 1; j < num_sockets; j++) {
              if (poll_fds[j].fd == poll_fds[i].fd || poll_fds[j].fd == timerfd)
                continue;

              int res = send_all(poll_fds[j].fd, &received_packet, sizeof(received_packet));
              DIE(res <= 0, "send_all error\n");
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("\n Usage: %s <ip> <port>\n", argv[0]);
    return 1;
  }

  // Convert port string to a number
  uint16_t port;
  int rc = sscanf(argv[2], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // TCP socket for client connections
  const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(listenfd < 0, "socket");

  // Filling serv_addr
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // Reusable socket address
  const int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Associate server address with the socket
  rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind");

  // run_chat_server(listenfd);
  run_chat_multi_server(listenfd);

  // Close listenfd socket
  close(listenfd);

  return 0;
}
