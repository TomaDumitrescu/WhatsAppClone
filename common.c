#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

// Receiving exactly len bytes from the buffer
int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;

  while (bytes_remaining) {
    int num_bytes = recv(sockfd, (void *)(buff + bytes_received), bytes_remaining, 0);
    if (num_bytes == -1)
      return bytes_received;

    if (num_bytes == 0)
      break;

    bytes_remaining -= num_bytes;
    bytes_received += (size_t)num_bytes;
  }

  return bytes_received;
}

// Sending exactly len bytes from the buffer
int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;

  while (bytes_remaining) {
    int num_bytes = send(sockfd, (void *)(buff + bytes_sent), bytes_remaining, 0);
    if (num_bytes == -1)
      return bytes_sent;

    bytes_remaining -= num_bytes;
    bytes_sent += (size_t)num_bytes;
  }

  return bytes_sent;
}
