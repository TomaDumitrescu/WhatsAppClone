#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <pthread.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32
#define MAX_MESSAGES 100
#define NUM_THREADS 4
#define MAX_MATCHES 5

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

typedef struct arg_search arg_search;
struct arg_search {
  int thread_id;
  char phrase[30];
};

int cnt = 0, matches = 0;
struct chat_packet history[MAX_MESSAGES], match_packets[MAX_MATCHES];

void *search(void *arg) {
  int id = ((arg_search *)(arg))->thread_id;
  int start = id * (double)cnt / NUM_THREADS;
  int end = MIN((id + 1) * (double)cnt / NUM_THREADS, cnt);

  for (int i = start; i < end; i++) {
    char *match = strstr(history[i].message, ((arg_search *)arg)->phrase);
    if (match) {
      match_packets[matches].len = strlen(history[i].message);
      strcpy(match_packets[matches++].message, history[i].message);
    }
  }

  return NULL;
}

void search_chat(char *phrase) {
  pthread_t threads[NUM_THREADS];
  matches = 0;

  arg_search args[NUM_THREADS];

  int phrase_len = strlen(phrase);
  for (int i = 0; i < NUM_THREADS; i++) {
    args[i].thread_id = i;
    strncpy(args[i].phrase, phrase, phrase_len);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    int rc = pthread_create(&threads[i], NULL, search,  (void *)&args[i]);
    DIE(rc == 1, "pthread_create error\n");
  }

  void *status;
  for (int i = 0; i < NUM_THREADS; i++) {
    int rc = pthread_join(threads[i], &status);
    DIE(rc == 1, "pthread_join error\n");
  }

  printf("Total matches %d\n", matches);
  for (int i = 0; i < matches; i++) {
    struct chat_packet *match = &match_packets[i];

    printf("Match %d:\n", i + 1);

    // Write only match->len characters since overwriting
    for (int j = 0; j < match->len; j++)
      printf("%c", match->message[j]);

    printf("\n");
  }
}

void run_chat_multi_server(int listenfd) {
  struct pollfd poll_fds[MAX_CONNECTIONS];
  int num_sockets = 3;
  int rc;
  char buf[MSG_MAXSIZE];

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

  poll_fds[2].fd = STDIN_FILENO;
  poll_fds[2].events = POLLIN;

  while (1) {
    // Waiting to receive something from one socket
    rc = poll(poll_fds, num_sockets, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_sockets; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == STDIN_FILENO) {
          // Processing input from keyboard
          if (fgets(buf, sizeof(buf), stdin) && !isspace(buf[0])) {
            if (strcmp(strtok(buf, " \n"), "search") == 0) {
              char *phrase = strtok(NULL, " \n");
              search_chat(phrase);
            }

            fflush(stdin);
          }
        }

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
                if (poll_fds[j].fd != timerfd && poll_fds[j].fd != listenfd
                    && poll_fds[j].fd != STDIN_FILENO) {
                    send_all(poll_fds[j].fd, &ad, sizeof(ad));
                }
            }
        } else {
          if (poll_fds[i].fd == STDIN_FILENO)
            continue;

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
              if (poll_fds[j].fd == poll_fds[i].fd || poll_fds[j].fd == timerfd
                  || poll_fds[j].fd == STDIN_FILENO)
                continue;

              int res = send_all(poll_fds[j].fd, &received_packet, sizeof(received_packet));
              DIE(res <= 0, "send_all error\n");
            }

            history[cnt].len = received_packet.len;
            strcpy(history[cnt].message, received_packet.message);
            if (cnt < MAX_MESSAGES)
              cnt++;
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

  run_chat_multi_server(listenfd);

  // Close listenfd socket
  close(listenfd);

  return 0;
}
