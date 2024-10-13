#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd, char *user_name) {
  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;

  // Reading multiplexing
  struct pollfd pfds[MAX_PFDS];
  int nfds = 0;

  // STDIN socket
  pfds[nfds].fd = STDIN_FILENO;
  pfds[nfds].events = POLLIN;
  nfds++;

  // Server socket
  pfds[nfds].fd = sockfd;
  pfds[nfds].events = POLLIN;
  nfds++;

  while (true) {
    int err = poll(pfds, nfds, -1);
    DIE(err == -1, "poll error");
    
    for (int i = 0; i < nfds; i++) {
      if (!(pfds[i].revents & POLLIN))
        continue;

      // Processing input from keyboard
      if (i == 0 && (fgets(buf, sizeof(buf), stdin) && !isspace(buf[0]))) {
        sent_packet.len = strlen(buf) + strlen(user_name) + 1;
        strcpy(sent_packet.message, user_name);
        strcat(sent_packet.message, buf);

        // Send the packet to the server
        send_all(sockfd, &sent_packet, sizeof(sent_packet));

        fflush(stdin);
      }

      if (i == 1) {
        // Print the message received from the server
        int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
        if (rc <= 0) {
          break;
        }

        printf("%s\n", recv_packet.message);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("\n Usage: %s <ip> <port> <user_name>\n", argv[0]);
    return 1;
  }

  char user_name[50];
  strcpy(user_name, argv[3]);
  strcat(user_name, ": ");

  // Convert port string to a number
  uint16_t port;
  int rc = sscanf(argv[2], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtaining a TCP socket
  const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  // Fill with the server address, address family and the port
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Connecting to the server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  run_client(sockfd, user_name);

  // Closing the connection and the socket
  close(sockfd);

  return 0;
}
