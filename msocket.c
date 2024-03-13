#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "msocket.h"
#define MAX_SOCKETS 10
#define MAX_MESSAGE_SIZE 1024

typedef struct {
    int sockfd;
    struct sockaddr_in src_addr;
    struct sockaddr_in dest_addr;
    int bound;
    char message_buffer[MAX_MESSAGE_SIZE];
    int message_length;
} SocketEntry;

SocketEntry socket_table[MAX_SOCKETS];

int m_socket(int domain, int type, int protocol) {
    if (type != SOCK_MTP) {
        errno = EINVAL;  // Unsupported socket type
        return -1;
    }

    // Find a free entry in the socket table
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == -1) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = ENOBUFS;  // No buffer space available
        return -1;
    }

    // Create the UDP socket
    int sockfd = socket(domain, SOCK_DGRAM, protocol);
    if (sockfd < 0) {
        return -1;
    }

    // Initialize the socket entry
    socket_table[i].sockfd = sockfd;
    socket_table[i].bound = 0;

    return sockfd;
}

int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Bind the UDP socket
    int ret = bind(sockfd, addr, addrlen);
    if (ret == 0) {
        memcpy(&socket_table[i].src_addr, addr, addrlen);
        socket_table[i].bound = 1;
    }

    return ret;
}

ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Check if the socket is bound
    if (!socket_table[i].bound) {
        errno = ENOTBOUND;
        return -1;
    }

    // Check if destination IP/Port matches with bound IP/Port
    if (memcmp(dest_addr, &socket_table[i].dest_addr, addrlen) != 0) {
        errno = ENOTBOUND;
        return -1;
    }

    // Check if there is space in the send buffer
    if (socket_table[i].message_length + len > MAX_MESSAGE_SIZE) {
        errno = ENOBUFS;
        return -1;
    }

    // Copy the message to the send buffer
    memcpy(socket_table[i].message_buffer + socket_table[i].message_length, buf, len);
    socket_table[i].message_length += len;

    return len;
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Check if there is any message in the receive buffer
    if (socket_table[i].message_length == 0) {
        errno = ENOMSG;
        return -1;
    }

    // Copy the message from the receive buffer
    memcpy(buf, socket_table[i].message_buffer, len);
    *addrlen = sizeof(socket_table[i].src_addr);
    memcpy(src_addr, &socket_table[i].src_addr, *addrlen);

    // Reset the receive buffer
    socket_table[i].message_length = 0;

    return len;
}

int m_close(int sockfd) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Close the UDP socket
    int ret = close(sockfd);
    if (ret == 0) {
        memset(&socket_table[i], 0, sizeof(SocketEntry));
        socket_table[i].sockfd = -1;
    }

    return ret;
}
